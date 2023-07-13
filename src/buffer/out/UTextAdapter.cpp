#include "precomp.h"

#include "UTextAdapter.h"

#include "textBuffer.hpp"
#include "../renderer/inc/IRenderData.hpp"

using Microsoft::Console::Render::IRenderData;

#define utextFieldChunkStart(ut) ut->p
#define utextFieldLength(ut) *reinterpret_cast<uintptr_t*>(&ut->r)
#define utextFieldRowStartIndex(ut) ut->a
#define utextFieldRow(ut) ut->b
#define utextFieldRowCount(ut) ut->c

static UText* U_CALLCONV utextClone(UText* dest, const UText* src, UBool deep, UErrorCode* status) noexcept
{
    if (deep)
    {
        *status = U_UNSUPPORTED_ERROR;
        return dest;
    }

    dest = utext_setup(dest, 0, status);
    if (U_SUCCESS(*status))
    {
        memcpy(dest, src, sizeof(UText));
    }

    return dest;
}

/**
 * Function type declaration for UText.nativeLength().
 *
 * @param ut the UText to get the length of.
 * @return the length, in the native units of the original text string.
 * @see UText
 * @stable ICU 3.4
 */
static int64_t U_CALLCONV utextLength(UText* ut) noexcept
{
    auto length = utextFieldLength(ut);

    if (!length)
    {
        const auto& renderData = *static_cast<const IRenderData*>(ut->context);
        const auto& textBuffer = renderData.GetTextBuffer();

        for (int32_t i = 0, c = ut->c; i < c; ++i)
        {
            length += textBuffer.GetRowByOffset(i).GetText().size();
        }

        utextFieldLength(ut) = length;
    }

    return gsl::narrow_cast<int64_t>(length);
}

/**
 * Function type declaration for UText.access().  Get the description of the text chunk
 *  containing the text at a requested native index.  The UText's iteration
 *  position will be left at the requested index.  If the index is out
 *  of bounds, the iteration position will be left at the start or end
 *  of the string, as appropriate.
 *
 *  Chunks must begin and end on code point boundaries.  A single code point
 *  comprised of multiple storage units must never span a chunk boundary.
 *
 *
 * @param ut          the UText being accessed.
 * @param nativeIndex Requested index of the text to be accessed.
 * @param forward     If true, then the returned chunk must contain text
 *                    starting from the index, so that start<=index<limit.
 *                    If false, then the returned chunk must contain text
 *                    before the index, so that start<index<=limit.
 * @return            True if the requested index could be accessed.  The chunk
 *                    will contain the requested text.
 *                    False value if a chunk cannot be accessed
 *                    (the requested index is out of bounds).
 *
 * @see UText
 * @stable ICU 3.4
 */
static UBool U_CALLCONV utextAccess(UText* ut, int64_t nativeIndex, UBool forward) noexcept
{
    if (!forward)
    {
        __debugbreak();
        nativeIndex--;
    }

    const auto& textBuffer = *static_cast<const TextBuffer*>(ut->context);
    auto start = ut->chunkNativeStart;
    auto limit = ut->chunkNativeLimit;
    std::wstring_view text;
    int32_t y = 0;

    if (nativeIndex >= start && nativeIndex < limit)
    {
        return true;
    }

    if (nativeIndex < start)
    {
        y = utextFieldRow(ut) - 1;

        for (;;)
        {
            if (y < 0)
            {
                return false;
            }

            text = textBuffer.GetRowByOffset(y).GetText();
            limit = start;
            start -= text.size();

            if (nativeIndex >= start)
            {
                break;
            }

            --y;
        }
    }
    else
    {
        const auto rowCount = utextFieldRowCount(ut);
        y = utextFieldRow(ut) + 1;

        for (;;)
        {
            if (y >= rowCount)
            {
                return false;
            }

            text = textBuffer.GetRowByOffset(y).GetText();
            start = limit;
            limit += text.size();

            if (nativeIndex < limit)
            {
                break;
            }

            ++y;
        }
    }

    utextFieldRow(ut) = y;
    ut->chunkNativeStart = start;
    ut->chunkNativeLimit = limit;
    ut->chunkOffset = gsl::narrow_cast<int32_t>(nativeIndex - start);
    ut->chunkLength = gsl::narrow_cast<int32_t>(text.size());
    ut->nativeIndexingLimit = ut->chunkLength;
    return true;
}

/**
 * Function type declaration for UText.extract().
 *
 * Extract text from a UText into a UChar buffer.  The range of text to be extracted
 * is specified in the native indices of the UText provider.  These may not necessarily
 * be UTF-16 indices.
 * <p>
 * The size (number of 16 bit UChars) in the data to be extracted is returned.  The
 * full amount is returned, even when the specified buffer size is smaller.
 * <p>
 * The extracted string will (if you are a user) / must (if you are a text provider)
 * be NUL-terminated if there is sufficient space in the destination buffer.
 *
 * @param  ut            the UText from which to extract data.
 * @param  nativeStart   the native index of the first character to extract.
 * @param  nativeLimit   the native string index of the position following the last
 *                       character to extract.
 * @param  dest          the UChar (UTF-16) buffer into which the extracted text is placed
 * @param  destCapacity  The size, in UChars, of the destination buffer.  May be zero
 *                       for precomputing the required size.
 * @param  status        receives any error status.
 *                       If U_BUFFER_OVERFLOW_ERROR: Returns number of UChars for
 *                       preflighting.
 * @return Number of UChars in the data.  Does not include a trailing NUL.
 *
 * @stable ICU 3.4
 */
static int32_t U_CALLCONV utextExtract(UText* ut, int64_t nativeStart, int64_t nativeLimit, char16_t* dest, int32_t destCapacity, UErrorCode* status) noexcept
{
    if (U_FAILURE(*status))
    {
        return 0;
    }
    if (destCapacity < 0 || (dest == nullptr && destCapacity > 0) || nativeStart > nativeLimit)
    {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    if (!utextAccess(ut, nativeStart, true))
    {
        return 0;
    }

    nativeLimit = std::min(ut->chunkNativeLimit, nativeLimit);

    if (destCapacity <= 0)
    {
        return gsl::narrow_cast<int32_t>(nativeLimit - nativeStart);
    }

    const auto& textBuffer = *static_cast<const TextBuffer*>(ut->context);
    const auto y = utextFieldRow(ut);
    const auto text = textBuffer.GetRowByOffset(y).GetText();

    const auto offset = ut->chunkNativeStart - nativeStart;
    const auto length = std::min(gsl::narrow_cast<size_t>(destCapacity), gsl::narrow_cast<size_t>(nativeLimit - ut->chunkNativeStart));
    memcpy(dest, text.data() + offset, length * sizeof(char16_t));

    if (length < destCapacity)
    {
        dest[length] = 0;
    }

    return gsl::narrow_cast<int32_t>(length);
}

static constexpr UTextFuncs ucstrFuncs{
    .tableSize = sizeof(UTextFuncs),
    .clone = utextClone,
    .nativeLength = utextLength,
    .access = utextAccess,
    .extract = utextExtract,
};

UText* UTextFromTextBuffer(UText* ut, const TextBuffer& textBuffer, UErrorCode* status) noexcept
{
    ut = utext_setup(ut, 0, status);
    if (U_FAILURE(*status))
    {
        return nullptr;
    }

    ut->providerProperties = UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE | UTEXT_PROVIDER_STABLE_CHUNKS;
    ut->pFuncs = &ucstrFuncs;
    ut->context = &textBuffer;
    utextFieldRowCount(ut) = textBuffer.GetSize().Height();
    return ut;
}

til::point_span BufferPositionFromUText(UText* ut, int64_t nativeIndex) noexcept
{
    if (!utextAccess(ut, nativeIndex, true))
    {
        return {};
    }

    const auto& textBuffer = *static_cast<const TextBuffer*>(ut->context);
    const auto y = utextFieldRow(ut);
    const auto offset = ut->chunkNativeStart - nativeIndex;
    const auto cols = textBuffer.GetRowByOffset(y).GetColumnSpanAtCharOffset(gsl::narrow_cast<uint16_t>(offset));
    return { { cols.first, y }, { cols.second, y } };
}
