/**
 *      File: PUTCH.C
 *      Super simple C example code for HELLDPMI
 *      Copyright (c) 2025 by Will Klees
 */

/**
 *  putch procedure - Outputs a single character to standard output.
 * 
 *  @param c: The ASCII code of the character to print.
 */
void putch(char c) {
    __asm {
        mov ah, 2           ; DOS API call: Output character to console
        mov dl, c
        int 21h
    }
}