/*
 * Copyright (C) 2010-2025 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "AccessibilityRenderObject.h"

namespace WebCore {

class AccessibilityMenuListPopup;
class RenderMenuList;

class AccessibilityMenuList final : public AccessibilityRenderObject {
public:
    static Ref<AccessibilityMenuList> create(AXID, RenderMenuList&, AXObjectCache&);

    bool isCollapsed() const final;
    bool press() final;

    void didUpdateActiveOption(int optionIndex);

private:
    explicit AccessibilityMenuList(AXID, RenderMenuList&, AXObjectCache&);

    bool isMenuList() const final { return true; }
    AccessibilityRole determineAccessibilityRole() final { return AccessibilityRole::PopUpButton; }

    bool canSetFocusAttribute() const final;
    void addChildren() final;
    void updateChildrenIfNecessary() final;
    // This class' children are initialized once in the constructor with m_popup.
    void clearChildren() final { };
    void setNeedsToUpdateChildren() final { };

    // FIXME: Nothing calls AXObjectCache::remove for m_popup.
    const Ref<AccessibilityMenuListPopup> m_popup;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AccessibilityMenuList) \
    static bool isType(const WebCore::AccessibilityObject& object) { return object.isMenuList(); } \
SPECIALIZE_TYPE_TRAITS_END()
