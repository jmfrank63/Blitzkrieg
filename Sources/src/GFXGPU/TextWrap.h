#ifndef BLITZKRIEG_TEXT_WRAP_H
#define BLITZKRIEG_TEXT_WRAP_H

#include "../GFX/TextBreak.h"
#include <cstddef>
#include <utility>
#include <vector>

typedef unsigned short WORD;

// The greedy line breaking behind WrapTextLines, over any font that can say how
// far a character advances and how wide a run is. Kept free of the font and
// engine types so a test can drive it with a fixed-width font.
//
// A line breaks at the last space, or, for CJK text that has none, at the last
// break opportunity of CanBreakTextBetween (kinsoku applied). Text without
// either breaks at the overflowing character, as it always did.
//
// TFont needs:  float Advance( WORD previous, WORD current ) const  - the
// A + kern + B + C of current after previous, summed in that order;
// float RunWidth( const WORD *text, int count ) const  - the same sum over a run.
template <class TFont>
void WrapTextLinesWith( const TFont *font, const WORD *text, float scale, float wrap_width,
    std::vector<std::pair<size_t, size_t> > &lines )
{
    const size_t none = static_cast<size_t>( -1 );
    lines.clear();
    if ( !text ) return;
    size_t length = 0;
    while ( text[length] ) ++length;
    const bool measure = font != 0 && wrap_width > 0.0f;
    size_t line_begin = 0;
    size_t last_space = none;
    // Where the line could break without a space: the index of the character
    // that would start the next line.
    size_t last_opportunity = none;
    // Running width of [line_begin, index]. Measuring the whole prefix again
    // for every character made the wrap quadratic, and a mission briefing pays
    // that on every frame it is on screen. The terms have to be added in
    // exactly the order FontGpu::TextWidth adds them: float addition is not
    // associative, and centered or right-aligned text derives its x from these
    // widths, so a single ulp of difference moves a line by a pixel.
    float run_width = 0.0f;
    WORD previous = 0;
    for ( size_t index = 0; index <= length; ++index )
    {
        // CRLF data would otherwise draw the carriage return as a missing glyph.
        if ( index == length || text[index] == L'\n' || text[index] == L'\r' )
        {
            lines.push_back( std::make_pair( line_begin, index ) );
            if ( index + 1 < length && text[index] == L'\r' && text[index + 1] == L'\n' ) ++index;
            line_begin = index + 1;
            last_space = none;
            last_opportunity = none;
            run_width = 0.0f;
            previous = 0;
            continue;
        }
        if ( text[index] == L' ' ) last_space = index;
        if ( !measure ) continue;
        const WORD current = text[index];
        if ( index > line_begin && current != L' ' && text[index - 1] != L' ' && CanBreakTextBetween( text[index - 1], current ) )
            last_opportunity = index;
        run_width += font->Advance( previous, current );
        previous = current;
        if ( index == line_begin ) continue;
        if ( run_width * scale <= wrap_width ) continue;
        const bool have_space = last_space != none && last_space > line_begin;
        const bool have_opportunity = last_opportunity != none && last_opportunity > line_begin;
        const bool break_on_space = have_space && ( !have_opportunity || last_space > last_opportunity );
        const size_t break_at = break_on_space ? last_space : ( have_opportunity ? last_opportunity : index );
        lines.push_back( std::make_pair( line_begin, break_at ) );
        line_begin = break_on_space ? break_at + 1 : break_at;
        last_space = none;
        last_opportunity = none;
        // The word that moved down has to be measured again from the start of
        // the new line, since the kerning against 0 differs from the kerning it
        // had mid-line. Every character is measured at most twice this way, so
        // the wrap stays linear. Breaking on a space that is itself the
        // overflowing character leaves nothing on the new line yet, so the
        // kerning partner has to go back to 0 with it.
        const size_t carried = line_begin <= index ? index - line_begin + 1 : 0;
        run_width = font->RunWidth( text + line_begin, static_cast<int>( carried ) );
        previous = carried != 0 ? text[index] : static_cast<WORD>( 0 );
    }
}

#endif
