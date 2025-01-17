/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "StylePrimitiveNumericTypes.h"

namespace WebCore {

class CSSValue;
class LayoutRect;
class LayoutUnit;
using LayoutBoxExtent = RectEdges<LayoutUnit>;

namespace CSS {
struct ScrollMarginEdge;
struct ScrollMargin;
}

namespace Style {

class BuilderState;

// <'scroll-margin-*'> = <length>
// https://drafts.csswg.org/css-scroll-snap-1/#margin-longhands-physical
DEFINE_TYPE_WRAPPER(ScrollMarginEdge, Length<>);

// <'scroll-margin'> = <length>{1,4}
// https://drafts.csswg.org/css-scroll-snap-1/#propdef-scroll-margin
DEFINE_TYPE_EXTENDER(ScrollMargin, SpaceSeparatedRectEdges<ScrollMarginEdge>);

DEFINE_TYPE_MAPPING(CSS::ScrollMarginEdge, ScrollMarginEdge)
DEFINE_TYPE_MAPPING(CSS::ScrollMargin, ScrollMargin)

// MARK: - Conversion

ScrollMarginEdge scrollMarginEdgeFromCSSValue(const CSSValue&, const BuilderState&);

// MARK: - Evaluation

LayoutBoxExtent extentForRect(const ScrollMargin&, const LayoutRect&);

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::ScrollMarginEdge)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_EXTENDER(WebCore::Style::ScrollMargin)
