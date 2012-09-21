/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "ScriptedAnimationController.h"

#if ENABLE(REQUEST_ANIMATION_FRAME)

#include "Document.h"
#include "Element.h"
#include "FrameView.h"
#include "RequestAnimationFrameCallback.h"

#if USE(REQUEST_ANIMATION_FRAME_TIMER)
#include <algorithm>
#include <wtf/CurrentTime.h>

using namespace std;

// Allow a little more than 60fps to make sure we can at least hit that frame rate.
#define MinimumAnimationInterval 0.015
#endif
namespace WebCore {

ScriptedAnimationController::ScriptedAnimationController(Document* document)
    : m_document(document)
    , m_nextCallbackId(0)
    , m_suspendCount(0)
#if USE(REQUEST_ANIMATION_FRAME_TIMER)
    , m_animationTimer(this, &ScriptedAnimationController::animationTimerFired)
    , m_lastAnimationFrameTime(0)
#endif
{
}

void ScriptedAnimationController::suspend()
{
    ++m_suspendCount;
}

void ScriptedAnimationController::resume()
{
    --m_suspendCount;
    if (!m_suspendCount && m_callbacks.size())
        if (FrameView* fv = m_document->view())
            fv->scheduleAnimation();
}

ScriptedAnimationController::CallbackId ScriptedAnimationController::registerCallback(PassRefPtr<RequestAnimationFrameCallback> callback, Element* animationElement)
{
    ScriptedAnimationController::CallbackId id = m_nextCallbackId++;
    callback->m_firedOrCancelled = false;
    callback->m_id = id;
    callback->m_element = animationElement;
    m_callbacks.append(callback);
    if (!m_suspendCount)
        scheduleAnimation();
    return id;
}

void ScriptedAnimationController::cancelCallback(CallbackId id)
{
    for (size_t i = 0; i < m_callbacks.size(); ++i) {
        if (m_callbacks[i]->m_id == id) {
            m_callbacks[i]->m_firedOrCancelled = true;
            m_callbacks.remove(i);
            return;
        }
    }
}

void ScriptedAnimationController::serviceScriptedAnimations(DOMTimeStamp time)
{
    if (!m_callbacks.size() || m_suspendCount)
        return;
    // We want to run the callback for all elements in the document that have registered
    // for a callback and that are visible.  Running the callbacks can cause new callbacks
    // to be registered, existing callbacks to be cancelled, and elements to gain or lose
    // visibility so this code has to iterate carefully.

    // FIXME: Currently, this code doesn't do any visibility tests beyond checking display:

    // First, generate a list of callbacks to consider.  Callbacks registered from this point
    // on are considered only for the "next" frame, not this one.
    CallbackList callbacks(m_callbacks);

    // Firing the callback may cause the visibility of other elements to change.  To avoid
    // missing any callbacks, we keep iterating through the list of candiate callbacks and firing
    // them until nothing new becomes visible.
    bool firedCallback;
    do {
        firedCallback = false;
        // A previous iteration may have invalidated style (or layout).  Update styles for each iteration
        // for now since all we check is the existence of a renderer.
        m_document->updateStyleIfNeeded();
        for (size_t i = 0; i < callbacks.size(); ++i) {
            RequestAnimationFrameCallback* callback = callbacks[i].get();
            if (!callback->m_firedOrCancelled && (!callback->m_element || callback->m_element->renderer())) {
                callback->m_firedOrCancelled = true;
                callback->handleEvent(time);
                firedCallback = true;
                callbacks.remove(i);
                break;
            }
        }
    } while (firedCallback);

    // Remove any callbacks we fired from the list of pending callbacks.
    for (size_t i = 0; i < m_callbacks.size();) {
        if (m_callbacks[i]->m_firedOrCancelled)
            m_callbacks.remove(i);
        else
            ++i;
    }

    if (m_callbacks.size())
        scheduleAnimation();
}

void ScriptedAnimationController::scheduleAnimation()
{
    if (!m_document)
        return;

#if USE(REQUEST_ANIMATION_FRAME_TIMER)
    if (m_animationTimer.isActive())
        return;
        
    double scheduleDelay = max<double>(MinimumAnimationInterval - (currentTime() - m_lastAnimationFrameTime), 0);
    m_animationTimer.startOneShot(scheduleDelay);
#else
    if (FrameView* frameView = m_document->view())
        frameView->scheduleAnimation();
#endif
}

#if USE(REQUEST_ANIMATION_FRAME_TIMER)
void ScriptedAnimationController::animationTimerFired(Timer<ScriptedAnimationController>*)
{
    m_lastAnimationFrameTime = currentTime();
    serviceScriptedAnimations(convertSecondsToDOMTimeStamp(m_lastAnimationFrameTime));
}
#endif

}

#endif
