package com.example.guitarpilot

import org.junit.Assert.assertEquals
import org.junit.Test

class ChordSessionGestureTest {
    @Test fun secondTapReturnsToOpenStrings() {
        assertEquals(0, ChordSession.chordAfterTap(8, 8)) // G → OPEN
        assertEquals(0, ChordSession.chordAfterTap(41, 41)) // Em7 → OPEN
    }

    @Test fun firstTapAfterOpenSelectsRequestedChord() {
        assertEquals(8, ChordSession.chordAfterTap(0, 8))
        assertEquals(41, ChordSession.chordAfterTap(0, 41))
    }

    @Test fun changingToAnotherChordDoesNotToggleOpen() {
        assertEquals(22, ChordSession.chordAfterTap(8, 22)) // G → Am
        assertEquals(8, ChordSession.chordAfterTap(22, 8)) // Am → G
    }

    @Test fun dedicatedOpenButtonRemainsOpen() {
        assertEquals(0, ChordSession.chordAfterTap(8, 0))
        assertEquals(0, ChordSession.chordAfterTap(0, 0))
    }

    @Test(expected = IllegalArgumentException::class)
    fun invalidChordIsRejected() {
        ChordSession.chordAfterTap(8, 49)
    }
}
