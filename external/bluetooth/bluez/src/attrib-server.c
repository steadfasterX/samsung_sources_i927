/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2010  Nokia Corporation
 *  Copyright (C) 2010  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/uuid.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include "log.h"
#include "gdbus.h"
#include "glib-compat.h"
#include "btio.h"
#include "sdpd.h"
#include "hcid.h"
#include "adapter.h"
#include "device.h"
#include "manager.h"
#include "att.h"
#include "gattrib.h"
#include "storage.h"

#include "attrib-server.h"

static GSList *servers = NULL;

struct gatt_server {
	struct btd_adapter *adapter;
	GIOChannel *l2cap_io;
	GIOChannel *le_io;
	uint32_t gatt_sdp_handle;
	uint32_t gap_sdp_handle;
	GList *database;
	GSList *clients;
	uint16_t name_handle;
	uint16_t appearance_handle;
};

struct gatt_channel {
	bdaddr_t src;
	bdaddr_t dst;
	GAttrib *attrib;
	guint mtu;
	gboolean le;
	guint id;
	gboolean encrypted;
	struct gatt_server *server;
	guint cleanup_id;
};

struct group_elem {
	uint16_t handle;
	uint16_t end;
	uint8_t *data;
	uint16_t len;
};

static bt_uuid_t prim_uuid = {
			.type = BT_UUID16,
			.value.u16 = GATT_PRIM_SVC_UUID
};
static bt_uuid_t snd_uuid = {
			.type = BT_UUID16,
			.value.u16 = GATT_SND_SVC_UUID
};
static bt_uuid_t ccc_uuid = {
			.type = BT_UUID16,
			.value.u16 = GATT_CLIENT_CHARAC_CFG_UUID
};

static void attrib_free(void *data)
{
	struct attribute *a = data;

	g_free(a->data);
	g_free(a);
}

static void channel_free(struct gatt_channel *channel)
{

	if (channel->cleanup_id)
		g_source_remove(channel->cleanup_id);

	g_attrib_unref(channel->attrib);
	g_free(channel);
}

static void gatt_server_free(struct gatt_server *server)
{
	g_list_free_full(server->database, attrib_free);

	if (server->l2cap_io != NULL) {
		g_io_channel_unref(server->l2cap_io);
		g_io_channel_shutdown(server->l2cap_io, FALSE, NULL);
	}

	if (server->le_io != NULL) {
		g_io_channel_unref(server->le_io);
		g_io_channel_shutdown(server->le_io, FALSE, NULL);
	}

	g_slist_free_full(server->clients, (GDestroyNotify) channel_free);

	if (server->gatt_sdp_handle > 0)
		remove_record_from_server(server->gatt_sdp_handle);

	if (server->gap_sdp_handle > 0)
		remove_record_from_server(server->gap_sdp_handle);

	if (server->adapter != NULL)
		btd_adapter_unref(server->adapter);

	g_free(server);
}

static gint adapter_cmp_addr(gconstpointer a, gconstpointer b)
{
	const struct gatt_server *server = a;
	const bdaddr_t *bdaddr = b;
	bdaddr_t src;

	adapter_get_address(server->adapter, &src);

	return bacmp(&src, bdaddr);
}

static gint adapter_cmp(gconstpointer a, gconstpointer b)
{
	const struct gatt_server *server = a;
	const struct btd_adapter *adapter = b;

	if (server->adapter == adapter)
		return 0;

	return -1;
}

static struct gatt_server *find_gatt_server(const bdaddr_t *bdaddr)
{
	GSList *l;

	l = g_slist_find_custom(servers, bdaddr, adapter_cmp_addr);
	if (l == NULL) {
		char addr[18];

		ba2str(bdaddr, addr);
		error("No GATT server found in %s", addr);
		return NULL;
	}

	return l->data;
}

static sdp_record_t *server_record_new(uuid_t *uuid, uint16_t start, uint16_t end)
{
	sdp_list_t *svclass_id, *apseq, *proto[2], *root, *aproto;
	uuid_t root_uuid, proto_uuid, l2cap;
	sdp_record_t *record;
	sdp_data_t *psm, *sh, *eh;
	uint16_t lp = ATT_PSM;

	if (uuid == NULL)
		return NULL;

	if (start > end)
		return NULL;

	record = sdp_record_alloc();
	if (record == NULL)
		return NULL;

	sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
	root = sdp_list_append(NULL, &root_uuid);
	sdp_set_browse_groups(record, root);
	sdp_list_free(root, NULL);

	svclass_id = sdp_list_append(NULL, uuid);
	sdp_set_service_classes(record, svclass_id);
	sdp_list_free(svclass_id, NULL);

	sdp_uuid16_create(&l2cap, L2CAP_UUID);
	proto[0] = sdp_list_append(NULL, &l2cap);
	psm = sdp_data_alloc(SDP_UINT16, &lp);
	proto[0] = sdp_list_append(proto[0], psm);
	apseq = sdp_list_append(NULL, proto[0]);

	sdp_uuid16_create(&proto_uuid, ATT_UUID);
	proto[1] = sdp_list_append(NULL, &proto_uuid);
	sh = sdp_data_alloc(SDP_UINT16, &start);
	proto[1] = sdp_list_append(proto[1], sh);
	eh = sdp_data_alloc(SDP_UINT16, &end);
	proto[1] = sdp_list_append(proto[1], eh);
	apseq = sdp_list_append(apseq, proto[1]);

	aproto = sdp_list_append(NULL, apseq);
	sdp_set_access_protos(record, aproto);

	sdp_data_free(psm);
	sdp_data_free(sh);
	sdp_data_free(eh);
	sdp_list_free(proto[0], NULL);
	sdp_list_free(proto[1], NULL);
	sdp_list_free(apseq, NULL);
	sdp_list_free(aproto, NULL);

	return record;
}

static int handle_cmp(gconstpointer a, gconstpointer b)
{
	const struct attribute *attrib = a;
	uint16_t handle = GPOINTER_TO_UINT(b);

	return attrib->handle - handle;
}

static int attribute_cmp(gconstpointer a1, gconstpointer a2)
{
	const struct attribute *attrib1 = a1;
	const struct attribute *attrib2 = a2;

	return attrib1->handle - attrib2->handle;
}

static struct attribute *find_primary_range(struct gatt_server *server,
						uint16_t start, uint16_t *end)
{
	struct attribute *attrib;
	guint h = start;
	GList *l;

	if (end == NULL)
		return NULL;

	l = g_list_find_custom(server->database, GUINT_TO_POINTER(h),
								handle_cmp);
	if (!l)
		return NULL;

	attrib = l->data;

	if (bt_uuid_cmp(&attrib->uuid, &prim_uuid) != 0)
		return NULL;

	*end = start;

	for (l = l->next; l; l = l->next) {
		struct attribute *a = l->data;

		if (bt_uuid_cmp(&a->uuid, &prim_uuid) == 0 ||
				bt_uuid_cmp(&a->uuid, &snd_uuid) == 0)
			break;

		*end = a->handle;
		}

	return attrib;
	}

static uint32_t attrib_create_sdp_new(struct gatt_server *server,
					uint16_t handle, const char *name)
{
	sdp_record_t *record;
	struct attribute *a;
	uint16_t end = 0;
	uuid_t svc, gap_uuid;
	bdaddr_t addr;

	a = find_primary_range(server, handle, &end);

	if (a == NULL)
		return 0;

	if (a->len == 2)
		sdp_uuid16_create(&svc, att_get_u16(a->data));
	else if (a->len == 16)
		sdp_uuid128_create(&svc, a->data);
	else
		return 0;

	record = server_record_new(&svc, handle, end);
	if (record == NULL)
		return 0;

	if (name != NULL)
		sdp_set_info_attr(record, name, "BlueZ", NULL);

	sdp_uuid16_create(&gap_uuid, GENERIC_ACCESS_PROFILE_ID);
	if (sdp_uuid_cmp(&svc, &gap_uuid) == 0) {
		sdp_set_url_attr(record, "http://www.bluez.org/",
				"http://www.bluez.org/",
				"http://www.bluez.org/");
	}

	adapter_get_address(server->adapter, &addr);
	if (add_record_to_server(&addr, record) == 0)
		return record->handle;

	sdp_record_free(record);
	return 0;
}

static struct attribute *attrib_db_add_new(struct gatt_server *server,
				uint16_t handle, bt_uuid_t *uuid, int read_reqs,
				int write_reqs, const uint8_t *value, int len)
{
	struct attribute *a;
	guint h = handle;

	DBG("handle=0x%04x", handle);

	if (g_list_find_custom(server->database, GUINT_TO_POINTER(h),
								handle_cmp))
		return NULL;

	a = g_new0(struct attribute, 1);
	a->len = len;
	a->data = g_memdup(value, len);
	a->handle = handle;
	a->uuid = *uuid;
	a->read_reqs = read_reqs;
	a->write_reqs = write_reqs;

	server->database = g_list_insert_sorted(server->database, a,
								attribute_cmp);

	return a;
}

static uint8_t att_check_reqs(struct gatt_channel *channel, uint8_t opcode,
								int reqs)
{
	/* FIXME: currently, it is assumed an encrypted link is enough for
	 * authentication. This will allow to enable the SMP negotiation once
	 * it is on upstream kernel. High security level should be mapped
	 * to authentication and medium to encryption permission. */
	if (!channel->encrypted)
		channel->encrypted = g_attrib_is_encrypted(channel->attrib);
	if (reqs == ATT_AUTHENTICATION && !channel->encrypted)
		return ATT_ECODE_AUTHENTICATION;
	else if (reqs == ATT_AUTHORIZATION)
		return ATT_ECODE_AUTHORIZATION;

	switch (opcode) {
	case ATT_OP_READ_BY_GROUP_REQ:
	case ATT_OP_READ_BY_TYPE_REQ:
	case ATT_OP_READ_REQ:
	case ATT_OP_READ_BLOB_REQ:
	case ATT_OP_READ_MULTI_REQ:
		if (reqs == ATT_NOT_PERMITTED)
			return ATT_ECODE_READ_NOT_PERM;
		break;
	case ATT_OP_PREP_WRITE_REQ:
	case ATT_OP_WRITE_REQ:
	case ATT_OP_WRITE_CMD:
		if (reqs == ATT_NOT_PERMITTED)
			return ATT_ECODE_WRITE_NOT_PERM;
		break;
	}

	return 0;
}

static uint16_t read_by_group(struct gatt_channel *channel, uint16_t start,
						uint16_t end, bt_uuid_t *uuid,
						uint8_t *pdu, int len)
{
	struct att_data_list *adl;
	struct attribute *a;
	struct group_elem *cur, *old = NULL;
	GSList *l, *groups;
	GList *dl, *database;
	uint16_t length, last_handle, last_size = 0;
	uint8_t status;
	int i;

	if (start > end || start == 0x0000)
		return enc_error_resp(ATT_OP_READ_BY_GROUP_REQ, start,
					ATT_ECODE_INVALID_HANDLE, pdu, len);

	/*
	 * Only <<Primary Service>> and <<Secondary Service>> grouping
	 * types may be used in the Read By Group Type Request.
	 */

	if (bt_uuid_cmp(uuid, &prim_uuid) != 0 &&
		bt_uuid_cmp(uuid, &snd_uuid) != 0)
		return enc_error_resp(ATT_OP_READ_BY_GROUP_REQ, 0x0000,
					ATT_ECODE_UNSUPP_GRP_TYPE, pdu, len);

	last_handle = end;
	database = channel->server->database;
	for (dl = database, groups = NULL, cur = NULL; dl; dl = dl->next) {

		a = dl->data;

		if (a->handle < start)
			continue;

		if (a->handle >= end)
			break;

		/* The old group ends when a new one starts */
		if (old && (bt_uuid_cmp(&a->uuid, &prim_uuid) == 0 ||
				bt_uuid_cmp(&a->uuid, &snd_uuid) == 0)) {
			old->end = last_handle;
			old = NULL;
		}

		if (bt_uuid_cmp(&a->uuid, uuid) != 0) {
			/* Still inside a service, update its last handle */
			if (old)
				last_handle = a->handle;
			continue;
		}

		if (last_size && (last_size != a->len))
			break;

		status = att_check_reqs(channel, ATT_OP_READ_BY_GROUP_REQ,
								a->read_reqs);

		if (status == 0x00 && a->read_cb)
			status = a->read_cb(a, a->cb_user_data);

		if (status) {
			g_slist_free_full(groups, g_free);
			return enc_error_resp(ATT_OP_READ_BY_GROUP_REQ,
						a->handle, status, pdu, len);
		}

		cur = g_new0(struct group_elem, 1);
		cur->handle = a->handle;
		cur->data = a->data;
		cur->len = a->len;

		/* Attribute Grouping Type found */
		groups = g_slist_append(groups, cur);

		last_size = a->len;
		old = cur;
		last_handle = cur->handle;
	}

	if (groups == NULL)
		return enc_error_resp(ATT_OP_READ_BY_GROUP_REQ, start,
					ATT_ECODE_ATTR_NOT_FOUND, pdu, len);

	if (dl == NULL)
		cur->end = a->handle;
	else
		cur->end = last_handle;

	length = g_slist_length(groups);

	adl = att_data_list_alloc(length, last_size + 4);

	for (i = 0, l = groups; l; l = l->next, i++) {
		uint8_t *value;

		cur = l->data;

		value = (void *) adl->data[i];

		att_put_u16(cur->handle, value);
		att_put_u16(cur->end, &value[2]);
		/* Attribute Value */
		memcpy(&value[4], cur->data, cur->len);
	}

	length = enc_read_by_grp_resp(adl, pdu, len);

	att_data_list_free(adl);
	g_slist_free_full(groups, g_free);

	return length;
}

static uint16_t read_by_type(struct gatt_channel *channel, uint16_t start,
						uint16_t end, bt_uuid_t *uuid,
						uint8_t *pdu, int len)
{
	struct att_data_list *adl;
	GSList *l, *types;
	GList *dl, *database;
	struct attribute *a;
	uint16_t num, length;
	uint8_t status;
	int i;

	if (start > end || start == 0x0000)
		return enc_error_resp(ATT_OP_READ_BY_TYPE_REQ, start,
					ATT_ECODE_INVALID_HANDLE, pdu, len);

	database = channel->server->database;
	for (dl = database, length = 0, types = NULL; dl; dl = dl->next) {

		a = dl->data;

		if (a->handle < start)
			continue;

		if (a->handle > end)
			break;

		if (bt_uuid_cmp(&a->uuid, uuid)  != 0)
			continue;

		status = att_check_reqs(channel, ATT_OP_READ_BY_TYPE_REQ,
								a->read_reqs);

		if (status == 0x00 && a->read_cb)
			status = a->read_cb(a, a->cb_user_data);

		if (status) {
			g_slist_free(types);
			return enc_error_resp(ATT_OP_READ_BY_TYPE_REQ,
						a->handle, status, pdu, len);
		}

		/* All elements must have the same length */
		if (length == 0)
			length = a->len;
		else if (a->len != length)
			break;

		types = g_slist_append(types, a);
	}

	if (types == NULL)
		return enc_error_resp(ATT_OP_READ_BY_TYPE_REQ, start,
					ATT_ECODE_ATTR_NOT_FOUND, pdu, len);

	num = g_slist_length(types);

	/* Handle length plus attribute value length */
	length += 2;

	adl = att_data_list_alloc(num, length);

	for (i = 0, l = types; l; i++, l = l->next) {
		uint8_t *value;

		a = l->data;

		value = (void *) adl->data[i];

		att_put_u16(a->handle, value);

		/* Attribute Value */
		memcpy(&value[2], a->data, a->len);
	}

	length = enc_read_by_type_resp(adl, pdu, len);

	att_data_list_free(adl);
	g_slist_free(types);

	return length;
}

static int find_info(struct gatt_channel *channel, uint16_t start, uint16_t end,
							uint8_t *pdu, int len)
{
	struct attribute *a;
	struct att_data_list *adl;
	GSList *l, *info;
	GList *dl, *database;
	uint8_t format, last_type = BT_UUID_UNSPEC;
	uint16_t length, num;
	int i;

	if (start > end || start == 0x0000)
		return enc_error_resp(ATT_OP_FIND_INFO_REQ, start,
					ATT_ECODE_INVALID_HANDLE, pdu, len);

	database = channel->server->database;
	for (dl = database, info = NULL, num = 0; dl; dl = dl->next) {
		a = dl->data;

		if (a->handle < start)
			continue;

		if (a->handle > end)
			break;

		if (last_type == BT_UUID_UNSPEC)
			last_type = a->uuid.type;

		if (a->uuid.type != last_type)
			break;

		info = g_slist_append(info, a);
		num++;

		last_type = a->uuid.type;
	}

	if (info == NULL)
		return enc_error_resp(ATT_OP_FIND_INFO_REQ, start,
					ATT_ECODE_ATTR_NOT_FOUND, pdu, len);

	if (last_type == BT_UUID16) {
		length = 2;
		format = 0x01;
	} else if (last_type == BT_UUID128) {
		length = 16;
		format = 0x02;
	} else {
		g_slist_free(info);
		return 0;
	}

	adl = att_data_list_alloc(num, length + 2);

	for (i = 0, l = info; l; i++, l = l->next) {
		uint8_t *value;

		a = l->data;

		value = (void *) adl->data[i];

		att_put_u16(a->handle, value);

		/* Attribute Value */
		att_put_uuid(a->uuid, &value[2]);
	}

	length = enc_find_info_resp(format, adl, pdu, len);

	att_data_list_free(adl);
	g_slist_free(info);

	return length;
}

static int find_by_type(struct gatt_channel *channel, uint16_t start,
			uint16_t end, bt_uuid_t *uuid, const uint8_t *value,
					int vlen, uint8_t *opdu, int mtu)
{
	struct attribute *a;
	struct att_range *range;
	GSList *matches;
	GList *dl, *database;
	int len;

	if (start > end || start == 0x0000)
		return enc_error_resp(ATT_OP_FIND_BY_TYPE_REQ, start,
					ATT_ECODE_INVALID_HANDLE, opdu, mtu);

	/* Searching first requested handle number */
	database = channel->server->database;
	for (dl = database, matches = NULL, range = NULL; dl; dl = dl->next) {
		a = dl->data;

		if (a->handle < start)
			continue;

		if (a->handle > end)
			break;

		/* Primary service? Attribute value matches? */
		if ((bt_uuid_cmp(&a->uuid, uuid) == 0) && (a->len == vlen) &&
					(memcmp(a->data, value, vlen) == 0)) {

			range = g_new0(struct att_range, 1);
			range->start = a->handle;
			/* It is allowed to have end group handle the same as
			 * start handle, for groups with only one attribute. */
			range->end = a->handle;

			matches = g_slist_append(matches, range);
		} else if (range) {
			/* Update the last found handle or reset the pointer
			 * to track that a new group started: Primary or
			 * Secondary service. */
			if (bt_uuid_cmp(&a->uuid, &prim_uuid) == 0 ||
					bt_uuid_cmp(&a->uuid, &snd_uuid) == 0)
				range = NULL;
			else
				range->end = a->handle;
		}
	}

	if (matches == NULL)
		return enc_error_resp(ATT_OP_FIND_BY_TYPE_REQ, start,
				ATT_ECODE_ATTR_NOT_FOUND, opdu, mtu);

	len = enc_find_by_type_resp(matches, opdu, mtu);

	g_slist_free_full(matches, g_free);

	return len;
}

static uint16_t read_value(struct gatt_channel *channel, uint16_t handle,
							uint8_t *pdu, int len)
{
	struct attribute *a;
	uint8_t status;
	GList *l;
	uint16_t cccval;
	guint h = handle;

	l = g_list_find_custom(channel->server->database,
					GUINT_TO_POINTER(h), handle_cmp);
	if (!l)
		return enc_error_resp(ATT_OP_READ_REQ, handle,
					ATT_ECODE_INVALID_HANDLE, pdu, len);

	a = l->data;

	if (bt_uuid_cmp(&ccc_uuid, &a->uuid) == 0 &&
		read_device_ccc(&channel->src, &channel->dst,
					handle, &cccval) == 0) {
		uint8_t config[2];

		att_put_u16(cccval, config);
		return enc_read_resp(config, sizeof(config), pdu, len);
	}

	status = att_check_reqs(channel, ATT_OP_READ_REQ, a->read_reqs);

	if (status == 0x00 && a->read_cb)
		status = a->read_cb(a, a->cb_user_data);

	if (status)
		return enc_error_resp(ATT_OP_READ_REQ, handle, status, pdu,
									len);

	return enc_read_resp(a->data, a->len, pdu, len);
}

static uint16_t read_blob(struct gatt_channel *channel, uint16_t handle,
					uint16_t offset, uint8_t *pdu, int len)
{
	struct attribute *a;
	uint8_t status;
	GList *l;
	uint16_t cccval;
	guint h = handle;

	l = g_list_find_custom(channel->server->database,
					GUINT_TO_POINTER(h), handle_cmp);
	if (!l)
		return enc_error_resp(ATT_OP_READ_BLOB_REQ, handle,
					ATT_ECODE_INVALID_HANDLE, pdu, len);

	a = l->data;

	if (a->len <= offset)
		return enc_error_resp(ATT_OP_READ_BLOB_REQ, handle,
					ATT_ECODE_INVALID_OFFSET, pdu, len);

	if (bt_uuid_cmp(&ccc_uuid, &a->uuid) == 0 &&
		read_device_ccc(&channel->src, &channel->dst,
					handle, &cccval) == 0) {
		uint8_t config[2];

		att_put_u16(cccval, config);
		return enc_read_blob_resp(config, sizeof(config), offset,
								pdu, len);
	}

	status = att_check_reqs(channel, ATT_OP_READ_BLOB_REQ, a->read_reqs);

	if (status == 0x00 && a->read_cb)
		status = a->read_cb(a, a->cb_user_data);

	if (status)
		return enc_error_resp(ATT_OP_READ_BLOB_REQ, handle, status,
								pdu, len);

	return enc_read_blob_resp(a->data, a->len, offset, pdu, len);
}

static uint16_t write_value(struct gatt_channel *channel, uint16_t handle,
						const uint8_t *value, int vlen,
						uint8_t *pdu, int len)
{
	struct attribute *a;
	uint8_t status;
	GList *l;
	guint h = handle;

	l = g_list_find_custom(channel->server->database,
					GUINT_TO_POINTER(h), handle_cmp);
	if (!l)
		return enc_error_resp(ATT_OP_WRITE_REQ, handle,
				ATT_ECODE_INVALID_HANDLE, pdu, len);

	a = l->data;

	status = att_check_reqs(channel, ATT_OP_WRITE_REQ, a->write_reqs);
	if (status)
		return enc_error_resp(ATT_OP_WRITE_REQ, handle, status, pdu,
									len);

	if (bt_uuid_cmp(&ccc_uuid, &a->uuid) != 0) {

		attrib_db_update(channel->server->adapter, handle, NULL,
							value, vlen, NULL);

	if (a->write_cb) {
		status = a->write_cb(a, a->cb_user_data);
		if (status)
				return enc_error_resp(ATT_OP_WRITE_REQ, handle,
							status, pdu, len);
		}
	} else {
		uint16_t cccval = att_get_u16(value);
		write_device_ccc(&channel->src, &channel->dst, handle, cccval);
	}

	return enc_write_resp(pdu, len);
}

static uint16_t mtu_exchange(struct gatt_channel *channel, uint16_t mtu,
		uint8_t *pdu, int len)
{
	guint old_mtu = channel->mtu;

	if (mtu < ATT_DEFAULT_LE_MTU)
		channel->mtu = ATT_DEFAULT_LE_MTU;
	else
		channel->mtu = MIN(mtu, channel->mtu);

	bt_io_set(channel->server->le_io, BT_IO_L2CAP, NULL,
			BT_IO_OPT_OMTU, channel->mtu,
			BT_IO_OPT_INVALID);

	return enc_mtu_resp(old_mtu, pdu, len);
}

static void channel_remove(struct gatt_channel *channel)
{
	channel->server->clients = g_slist_remove(channel->server->clients,
								channel);
	channel_free(channel);
}

static gboolean channel_watch_cb(GIOChannel *io, GIOCondition cond,
						gpointer user_data)
{
	channel_remove(user_data);

	return FALSE;
}

static void channel_handler(const uint8_t *ipdu, uint16_t len,
							gpointer user_data)
{
	struct gatt_channel *channel = user_data;
	uint8_t opdu[ATT_MAX_MTU], value[ATT_MAX_MTU];
	uint16_t length, start, end, mtu, offset;
	bt_uuid_t uuid;
	uint8_t status = 0;
	int vlen;

	DBG("op 0x%02x", ipdu[0]);

	switch (ipdu[0]) {
	case ATT_OP_READ_BY_GROUP_REQ:
		length = dec_read_by_grp_req(ipdu, len, &start, &end, &uuid);
		if (length == 0) {
			status = ATT_ECODE_INVALID_PDU;
			goto done;
		}

		length = read_by_group(channel, start, end, &uuid, opdu,
								channel->mtu);
		break;
	case ATT_OP_READ_BY_TYPE_REQ:
		length = dec_read_by_type_req(ipdu, len, &start, &end, &uuid);
		if (length == 0) {
			status = ATT_ECODE_INVALID_PDU;
			goto done;
		}

		length = read_by_type(channel, start, end, &uuid, opdu,
								channel->mtu);
		break;
	case ATT_OP_READ_REQ:
		length = dec_read_req(ipdu, len, &start);
		if (length == 0) {
			status = ATT_ECODE_INVALID_PDU;
			goto done;
		}

		length = read_value(channel, start, opdu, channel->mtu);
		break;
	case ATT_OP_READ_BLOB_REQ:
		length = dec_read_blob_req(ipdu, len, &start, &offset);
		if (length == 0) {
			status = ATT_ECODE_INVALID_PDU;
			goto done;
		}

		length = read_blob(channel, start, offset, opdu, channel->mtu);
		break;
	case ATT_OP_MTU_REQ:
		if (!channel->le) {
			status = ATT_ECODE_REQ_NOT_SUPP;
			goto done;
		}

		length = dec_mtu_req(ipdu, len, &mtu);
		if (length == 0) {
			status = ATT_ECODE_INVALID_PDU;
			goto done;
		}

		length = mtu_exchange(channel, mtu, opdu, channel->mtu);
		break;
	case ATT_OP_FIND_INFO_REQ:
		length = dec_find_info_req(ipdu, len, &start, &end);
		if (length == 0) {
			status = ATT_ECODE_INVALID_PDU;
			goto done;
		}

		length = find_info(channel, start, end, opdu, channel->mtu);
		break;
	case ATT_OP_WRITE_REQ:
		length = dec_write_req(ipdu, len, &start, value, &vlen);
		if (length == 0) {
			status = ATT_ECODE_INVALID_PDU;
			goto done;
		}

		length = write_value(channel, start, value, vlen, opdu,
								channel->mtu);
		break;
	case ATT_OP_WRITE_CMD:
		length = dec_write_cmd(ipdu, len, &start, value, &vlen);
		if (length > 0)
			write_value(channel, start, value, vlen, opdu,
								channel->mtu);
		return;
	case ATT_OP_FIND_BY_TYPE_REQ:
		length = dec_find_by_type_req(ipdu, len, &start, &end,
							&uuid, value, &vlen);
		if (length == 0) {
			status = ATT_ECODE_INVALID_PDU;
			goto done;
		}

		length = find_by_type(channel, start, end, &uuid, value, vlen,
							opdu, channel->mtu);
		break;
	case ATT_OP_HANDLE_CNF:
		return;
	case ATT_OP_READ_MULTI_REQ:
	case ATT_OP_PREP_WRITE_REQ:
	case ATT_OP_EXEC_WRITE_REQ:
	default:
		DBG("Unsupported request 0x%02x", ipdu[0]);
		status = ATT_ECODE_REQ_NOT_SUPP;
		goto done;
	}

	if (length == 0)
		status = ATT_ECODE_IO;

done:
	if (status)
		length = enc_error_resp(ipdu[0], 0x0000, status, opdu,
								channel->mtu);

	g_attrib_send(channel->attrib, 0, opdu[0], opdu, length,
							NULL, NULL, NULL);
}

guint attrib_channel_attach(GAttrib *attrib)
{
	struct gatt_server *server;
	struct btd_device *device;
	struct gatt_channel *channel;
	GIOChannel *io;
	GError *gerr = NULL;
	char addr[18];
	uint16_t cid;

	io = g_attrib_get_channel(attrib);

	channel = g_new0(struct gatt_channel, 1);

	bt_io_get(io, BT_IO_L2CAP, &gerr,
			BT_IO_OPT_SOURCE_BDADDR, &channel->src,
			BT_IO_OPT_DEST_BDADDR, &channel->dst,
			BT_IO_OPT_CID, &cid,
			BT_IO_OPT_OMTU, &channel->mtu,
			BT_IO_OPT_INVALID);
	if (gerr) {
		error("bt_io_get: %s", gerr->message);
		g_error_free(gerr);
		g_free(channel);
		return 0;
	}

	server = find_gatt_server(&channel->src);
	if (server == NULL) {
		char src[18];

		ba2str(&channel->src, src);
		error("No GATT server found in %s", src);
		g_free(channel);
		return 0;
	}

	channel->server = server;

	ba2str(&channel->dst, addr);

	device = adapter_find_device(server->adapter, addr);
	if (device == NULL || device_is_bonded(device) == FALSE)
		delete_device_ccc(&channel->src, &channel->dst);

	if (channel->mtu > ATT_MAX_MTU)
		channel->mtu = ATT_MAX_MTU;

	if (cid != ATT_CID)
		channel->le = FALSE;
	else
		channel->le = TRUE;


	channel->attrib = g_attrib_ref(attrib);
	channel->id = g_attrib_register(channel->attrib, GATTRIB_ALL_REQS,
				channel_handler, channel, NULL);

	channel->cleanup_id = g_io_add_watch(io, G_IO_HUP, channel_watch_cb,
								channel);

	server->clients = g_slist_append(server->clients, channel);

	return channel->id;
}

static gint channel_id_cmp(gconstpointer data, gconstpointer user_data)
{
	const struct gatt_channel *channel = data;
	guint id = GPOINTER_TO_UINT(user_data);

	return channel->id - id;
}

gboolean attrib_channel_detach(GAttrib *attrib, guint id)
{
	struct gatt_server *server = NULL;
	struct gatt_channel *channel;
	GError *gerr = NULL;
	GIOChannel *io;
	bdaddr_t src;
	GSList *l;

	io = g_attrib_get_channel(attrib);

	bt_io_get(io, BT_IO_L2CAP, &gerr, BT_IO_OPT_SOURCE_BDADDR, &src,
							BT_IO_OPT_INVALID);

	if (gerr != NULL) {
		error("bt_io_get: %s", gerr->message);
		g_error_free(gerr);
		return FALSE;
	}

	server = find_gatt_server(&src);

	if (server == NULL)
		return FALSE;

	l = g_slist_find_custom(server->clients, GUINT_TO_POINTER(id),
								channel_id_cmp);
	if (!l)
		return FALSE;

	channel = l->data;

	g_attrib_unregister(channel->attrib, channel->id);
	channel_remove(channel);

	return TRUE;
		}

static void connect_event(GIOChannel *io, GError *gerr, void *user_data)
{
	GAttrib *attrib;

	if (gerr) {
		error("%s", gerr->message);
				return;
	}

	attrib = g_attrib_new(io);
	attrib_channel_attach(attrib);
	g_attrib_unref(attrib);
		}

static void confirm_event(GIOChannel *io, void *user_data)
{
	GError *gerr = NULL;

	if (bt_io_accept(io, connect_event, user_data, NULL, &gerr) == FALSE) {
		error("bt_io_accept: %s", gerr->message);
		g_error_free(gerr);
		g_io_channel_unref(io);
	}

	return;
}

static gboolean register_core_services(struct gatt_server *server)
{
	uint8_t atval[256];
	bt_uuid_t uuid;
	uint16_t appearance = 0x0000;

	/* GAP service: primary service definition */
	bt_uuid16_create(&uuid, GATT_PRIM_SVC_UUID);
	att_put_u16(GENERIC_ACCESS_PROFILE_ID, &atval[0]);
	attrib_db_add_new(server, 0x0001, &uuid, ATT_NONE, ATT_NOT_PERMITTED,
								atval, 2);

	/* GAP service: device name characteristic */
	server->name_handle = 0x0006;
	bt_uuid16_create(&uuid, GATT_CHARAC_UUID);
	atval[0] = ATT_CHAR_PROPER_READ;
	att_put_u16(server->name_handle, &atval[1]);
	att_put_u16(GATT_CHARAC_DEVICE_NAME, &atval[3]);
	attrib_db_add_new(server, 0x0004, &uuid, ATT_NONE, ATT_NOT_PERMITTED,
								atval, 5);

	/* GAP service: device name attribute */
	bt_uuid16_create(&uuid, GATT_CHARAC_DEVICE_NAME);
	attrib_db_add_new(server, server->name_handle, &uuid, ATT_NONE,
						ATT_NOT_PERMITTED, NULL, 0);

	/* GAP service: device appearance characteristic */
	server->appearance_handle = 0x0008;
	bt_uuid16_create(&uuid, GATT_CHARAC_UUID);
	atval[0] = ATT_CHAR_PROPER_READ;
	att_put_u16(server->appearance_handle, &atval[1]);
	att_put_u16(GATT_CHARAC_APPEARANCE, &atval[3]);
	attrib_db_add_new(server, 0x0007, &uuid, ATT_NONE, ATT_NOT_PERMITTED,
								atval, 5);

	/* GAP service: device appearance attribute */
	bt_uuid16_create(&uuid, GATT_CHARAC_APPEARANCE);
	att_put_u16(appearance, &atval[0]);
	attrib_db_add_new(server, server->appearance_handle, &uuid, ATT_NONE,
						ATT_NOT_PERMITTED, atval, 2);
	server->gap_sdp_handle = attrib_create_sdp_new(server, 0x0001,
						"Generic Access Profile");
	if (server->gap_sdp_handle == 0) {
		error("Failed to register GAP service record");
		return FALSE;
	}

	/* GATT service: primary service definition */
	bt_uuid16_create(&uuid, GATT_PRIM_SVC_UUID);
	att_put_u16(GENERIC_ATTRIB_PROFILE_ID, &atval[0]);
	attrib_db_add_new(server, 0x0010, &uuid, ATT_NONE, ATT_NOT_PERMITTED,
								atval, 2);

	server->gatt_sdp_handle = attrib_create_sdp_new(server, 0x0010,
						"Generic Attribute Profile");
	if (server->gatt_sdp_handle == 0) {
		error("Failed to register GATT service record");
		return FALSE;
	}

	return TRUE;
}

int btd_adapter_gatt_server_start(struct btd_adapter *adapter)
{
	struct gatt_server *server;
	GError *gerr = NULL;
	bdaddr_t addr;

	DBG("Start GATT server in hci%d", adapter_get_dev_id(adapter));

	server = g_new0(struct gatt_server, 1);
	server->adapter = btd_adapter_ref(adapter);

	adapter_get_address(server->adapter, &addr);

/* remove sdp,psm gap/gatt in BR/EDR 
	// BR/EDR socket
	server->l2cap_io = bt_io_listen(BT_IO_L2CAP, NULL, confirm_event,
					NULL, NULL, &gerr,
					BT_IO_OPT_SOURCE_BDADDR, &addr,
					BT_IO_OPT_PSM, ATT_PSM,
					BT_IO_OPT_SEC_LEVEL, BT_IO_SEC_LOW,
					BT_IO_OPT_INVALID);

	if (server->l2cap_io == NULL) {
		error("%s", gerr->message);
		g_error_free(gerr);
		gatt_server_free(server);
		return -1;
	}



	if (!register_core_services(server)) {
		gatt_server_free(server);
		return -1;
	}
*/

	/* LE socket */
	server->le_io = bt_io_listen(BT_IO_L2CAP, NULL, confirm_event,
					&server->le_io, NULL, &gerr,
					BT_IO_OPT_SOURCE_BDADDR, &addr,
					BT_IO_OPT_CID, ATT_CID,
					BT_IO_OPT_SEC_LEVEL, BT_IO_SEC_LOW,
					BT_IO_OPT_INVALID);

	if (server->le_io == NULL) {
		error("%s", gerr->message);
		g_error_free(gerr);
		/* Doesn't have LE support, continue */
	}

	servers = g_slist_prepend(servers, server);
	return 0;
}

void btd_adapter_gatt_server_stop(struct btd_adapter *adapter)
{
	struct gatt_server *server;
	GSList *l;

	l = g_slist_find_custom(servers, adapter, adapter_cmp);
	if (l == NULL)
		return;

	DBG("Stop GATT server in hci%d", adapter_get_dev_id(adapter));

	server = l->data;
	servers = g_slist_remove(servers, server);
	gatt_server_free(server);
	}

uint32_t attrib_create_sdp(struct btd_adapter *adapter, uint16_t handle,
							const char *name)
{
	GSList *l;

	l = g_slist_find_custom(servers, adapter, adapter_cmp);
	if (l == NULL)
		return 0;

	return attrib_create_sdp_new(l->data, handle, name);
	}

void attrib_free_sdp(uint32_t sdp_handle)
{
	remove_record_from_server(sdp_handle);
}

static uint16_t find_uuid16_avail(struct btd_adapter *adapter, uint16_t nitems)
{
	struct gatt_server *server;
	uint16_t handle;
	GSList *l;
	GList *dl;

	l = g_slist_find_custom(servers, adapter, adapter_cmp);
	if (l == NULL)
		return 0;

	server = l->data;
	if (server->database == NULL)
		return 0x0001;

	for (dl = server->database, handle = 0x0001; dl; dl = dl->next) {
		struct attribute *a = dl->data;

		if ((bt_uuid_cmp(&a->uuid, &prim_uuid) == 0 ||
				bt_uuid_cmp(&a->uuid, &snd_uuid) == 0) &&
				a->handle - handle >= nitems)
			/* Note: the range above excludes the current handle */
			return handle;

		if (a->len == 16 && (bt_uuid_cmp(&a->uuid, &prim_uuid) == 0 ||
				bt_uuid_cmp(&a->uuid, &snd_uuid) == 0)) {
			/* 128 bit UUID service definition */
		return 0;
		}

		if (a->handle == 0xffff)
		return 0;

		handle = a->handle + 1;
	}

	if (0xffff - handle + 1 >= nitems)
		return handle;

	return 0;
}

static uint16_t find_uuid128_avail(struct btd_adapter *adapter, uint16_t nitems)
{
	uint16_t handle = 0, end = 0xffff;
	struct gatt_server *server;
	GList *dl;
	GSList *l;

	l = g_slist_find_custom(servers, adapter, adapter_cmp);
	if (l == NULL)
		return 0;

	server = l->data;
	if (server->database == NULL)
		return 0xffff - nitems + 1;

	for (dl = g_list_last(server->database); dl; dl = dl->prev) {
		struct attribute *a = dl->data;

		if (handle == 0)
			handle = a->handle;

		if (bt_uuid_cmp(&a->uuid, &prim_uuid) != 0 &&
				bt_uuid_cmp(&a->uuid, &snd_uuid) != 0)
			continue;

		if (end - handle >= nitems)
			return end - nitems + 1;

		if (a->len == 2) {
			/* 16 bit UUID service definition */
			return 0;
		}

		if (a->handle == 0x0001)
			return 0;

		end = a->handle - 1;
		handle = 0;
	}

	if (end - 0x0001 >= nitems)
		return end - nitems + 1;

	return 0;
}

uint16_t attrib_db_find_avail(struct btd_adapter *adapter, bt_uuid_t *svc_uuid,
								uint16_t nitems)
{
	g_assert(nitems > 0);

	if (svc_uuid->type == BT_UUID16)
		return find_uuid16_avail(adapter, nitems);
	else if (svc_uuid->type == BT_UUID128)
		return find_uuid128_avail(adapter, nitems);
	else {
		char uuidstr[MAX_LEN_UUID_STR];

		bt_uuid_to_string(svc_uuid, uuidstr, MAX_LEN_UUID_STR);
		error("Service uuid: %s is neither a 16-bit nor a 128-bit uuid",
								uuidstr);
		return 0;
	}
}

struct attribute *attrib_db_add(struct btd_adapter *adapter, uint16_t handle,
				bt_uuid_t *uuid, int read_reqs, int write_reqs,
						const uint8_t *value, int len)
{
	GSList *l;

	l = g_slist_find_custom(servers, adapter, adapter_cmp);
	if (l == NULL)
		return NULL;

	return attrib_db_add_new(l->data, handle, uuid, read_reqs, write_reqs,
								value, len);
}

int attrib_db_update(struct btd_adapter *adapter, uint16_t handle,
					bt_uuid_t *uuid, const uint8_t *value,
					int len, struct attribute **attr)
{
	struct gatt_server *server;
	struct attribute *a;
	GSList *l;
	GList *dl;
	guint h = handle;

	l = g_slist_find_custom(servers, adapter, adapter_cmp);
	if (l == NULL)
		return -ENOENT;

	server = l->data;

	DBG("handle=0x%04x", handle);

	dl = g_list_find_custom(server->database, GUINT_TO_POINTER(h),
								handle_cmp);
	if (dl == NULL)
		return -ENOENT;

	a = dl->data;

	a->data = g_try_realloc(a->data, len);
	if (a->data == NULL)
		return -ENOMEM;

	a->len = len;
	memcpy(a->data, value, len);

	if (uuid != NULL)
		a->uuid = *uuid;

	if (attr)
		*attr = a;

	return 0;
}

int attrib_db_del(struct btd_adapter *adapter, uint16_t handle)
{
	struct gatt_server *server;
	struct attribute *a;
	GSList *l;
	GList *dl;
	guint h = handle;

	l = g_slist_find_custom(servers, adapter, adapter_cmp);
	if (l == NULL)
		return -ENOENT;

	server = l->data;

	DBG("handle=0x%04x", handle);

	dl = g_list_find_custom(server->database, GUINT_TO_POINTER(h),
								handle_cmp);
	if (dl == NULL)
		return -ENOENT;

	a = dl->data;
	server->database = g_list_remove(server->database, a);
	g_free(a->data);
	g_free(a);

	return 0;
}

int attrib_gap_set(struct btd_adapter *adapter, uint16_t uuid,
						const uint8_t *value, int len)
{
	struct gatt_server *server;
	uint16_t handle;
	GSList *l;

	l = g_slist_find_custom(servers, adapter, adapter_cmp);
	if (l == NULL)
		return -ENOENT;

	server = l->data;

	/* FIXME: Missing Privacy and Reconnection Address */

	switch (uuid) {
	case GATT_CHARAC_DEVICE_NAME:
		handle = server->name_handle;
		break;
	case GATT_CHARAC_APPEARANCE:
		handle = server->appearance_handle;
		break;
	default:
		return -ENOSYS;
	}

	return attrib_db_update(adapter, handle, NULL, value, len, NULL);
}
