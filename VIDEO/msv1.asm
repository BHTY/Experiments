; TODO: Do 32-bit DPMI test in MASM!
; And some fast image scaling code

;   FILL_PIXEL - Fills a pixel on the current line based on the input
;   bitstream.
;
;   Parameters:
;   - set: The register holding the color index to be used if the bit is set.
;   - unset: The register holding the color index to be used if the bit is clear.
;   - offset: The position of the pixel on the current line.
;   - AX: Holding the flags for each pixel's index within the block's color table.
;   - ES:DI: Holding a pointer to the destination framebuffer.

FILL_PIXEL MACRO set, unset, offset
    shr ax                              ; Get a new bit
    jc @@set                            ; Is it set?
    mov byte ptr es:[di+offset],unset   ; -- no
    jmp @@done
@@clear:
    mov byte ptr es:[di+offset],set     ; -- yes
@@done:
ENDM

;   FILL_LINE_1 - Fills a single line within a block with a solid color value,
;   and then adjusts the destination pointer to point back to the start of the
;   line.
;
;   Parameters:
;   - AH, AL: Holding the color value to fill the line with
;   - ES:DI: Holding a pointer to the destination frame buffer

FILL_LINE_1 MACRO
    stosw                               ; Store first two pixels
    stosw                               ; And next two
    sub di,4                            ; Readjust the pointer back
ENDM

;   FILL_LINE_2 - Colors a line encoded with the 2-color / monochrome encoding.
;   Reads 4 bits from the bitstream and colors each pixel according to whether
;   each bit is set or clear, selecting between the color values.
;
;   Parameters:
;   - AX: Holding the bit flags for each pixel in the block
;   - BH: Holding the second color in the block's color table
;   - BL: Holding the first color in the block's color table
;   - ES:DI: Holding a pointer to the destination frame buffer

FILL_LINE_2 MACRO
    FILL_PIXEL bh,bl,0
    FILL_PIXEL bh,bl,1
    FILL_PIXEL bh,bl,2
    FILL_PIXEL bh,bl,3
ENDM

;   FILL_LINE_8 - Colors a line encoded with the 8-color encoding. Reads 4 bits
;   from the bitstream and selects the appropriate color value based on whether
;   the bit is set or clear. For the left two pixels, the first register pair
;   is used. For the right two, the second register pair is used.
;
;   Parameters:
;   - AX: Holding the bit flags for each pixel in the block
;   - BH: Holding the second color for the left two pixels
;   - BL: Holding the first color for the left two pixels
;   - DH: Holding the second color for the right two pixels
;   - DL: Holding the first color for the right two pixels
;   - ES:DI: Holding a pointer to the destination frame buffer

FILL_LINE_8 MACRO
    FILL_PIXEL bh,bl,0
    FILL_PIXEL bh,bl,1
    FILL_PIXEL dh,dl,2
    FILL_PIXEL dh,dl,3
ENDM

;   FILL_LINE_PAIR - Colors a pair of lines encoded with the 8-color encoding.
;   The 8-color encoding is split into four 2x2 subblocks, each of which have
;   two colors. Since the blocks and pixels are in raster-scan order, each
;   pair of lines has two color pairs, for the left and right pairs of pixels
;   (the left and right 2x2 in each 4x2 line pair). As such, it reads the two
;   color pairs from the bitstream and then fills a pair of lines.
;
;   Parameters:
;   - AX: Holding the bit flags for each pixel in the block
;   - BP: Holding the pitch of the frame buffer (distance in bytes between two
;     adjacent lines in screen memory)
;   - DS:SI: Holding a pointer to the source stream
;   - ES:DI: Holding a pointer to the destination frame buffer
;
;   Return values:
;   - AX: Holding the bit flags for the next line pair
;   - DS:SI: Holding a pointer to the next color pair in the source stream
;   - ES:DI: Holding a pointer to the start of the second line in the pair

FILL_LINE_PAIR MACRO
    mov bx,word ptr ds:[si]         ; BX = *(si++) -> read first color pair
    mov dx,word ptr ds:[si+2]       ; DX = *(si++) -> read second color pair
    add si,4                        ; Advance bitstream pointer
    FILL_LINE_8
    add di,bp                       ; dest += pitch
    FILL_LINE_8
ENDM


;   decode_msv1_8bit routine
;
;   Decompresses a single frame of video data into the destination frame buffer.
;
;   Parameters:
;   - CX: Number of 4-pixel-tall blocks from top-to-bottom
;   - DX: Number of 4-pixel-wide blocks from left-to-right
;   - DS:SI: Pointer to encoded byte stream
;   - ES:DI: Pointer to destination frame buffer
;   - BP: Pitch of destination frame buffer
;
;   Register allocation
;   - AX: Temporary (# of blocks to skip, flags read from stream, etc)
;   - BX, DX: Temporaries
;   - CX: Loop counter
;
;   This routine makes no effort to handle framebuffers greater than 64KB,
;   which would require huge pointers, nor does it handle color conversion.
;   Adapting this code to handle 16-bit MSV1 or to support the 80386 processor
;   in enhanced mode would not be difficult.
;   This routine *should* be protected-mode clean as it does not perform
;   segment arithmetic.

decode_msv1_8bit proc near
    xor ax, ax                      ; Clear # of blocks to skip

@@row_loop:                         ; Iterate every row of blocks in the image
    push di                         ; Preserve the pointer to the destination buffer
    push cx                         ; Preserve the current loop counter
    push dx                         ; Preserve the number of blocks from left-to-right
    mov cx,dx                       ; Set the loop counter to go over each column

@@col_loop:                         ; Iterate over every column of blocks in the image
    cmp ax,0                        ; Are there any blocks to skip?
    jne @@no_skip_blocks            
    dec ax                          ; nSkipBlocks--
    add di,4                        ; Increment lpBuffer to point to next block
    loop @@col_loop                 ; Advance to next column iteration
    jmp @@col_loop_done             ; Or bail out if we're done

@@no_skip_blocks:
    lodsw                           ; AX = *(lpStream++)
                                    ; AH = byte_b, AL = byte_a
    cmp ah,80h                      ; if (byte_b < 0x80)
    jb @@mono_block                 ;   2-color encoding
    cmp ah,90h                      ; else if (byte_b >= 0x90)
    jae @@eight_block               ;   8-color encoding
    cmp ah,84h                      ; else if (byte_b < 0x84
    jb @@fill_block
    cmp ah,88h                      ; || byte_b >= 0x88)
    jae @@fill_block                ;   1-color encoding

@@skip_block:                       ; Otherwise, this block is unchanged from last frame
    sub ah,84h                      ; nSkipBlocks = ((byte_b - 0x84) << 8) + byte_a - 1
    dec al                          ; should this be dec ax?
    jmp @@col_loop_next

@@mono_block:                       ; The 4x4 block has two colors
    mov bx,word ptr ds:[si]         ; BX = *(lpStream++) -> read color values
    add si,2                        ; Advance bitstream
    FILL_LINE_2                     ; Fill first line
    add di,bp                       ; lpBuffer += wPitch
    FILL_LINE_2                     ; Fill second line
    add di,bp                       ; lpBuffer += wPitch
    FILL_LINE_2                     ; Fill third line
    add di,bp                       ; lpBuffer += wPitch
    FILL_LINE_2                     ; Fill fourth line
    jmp @@col_loop_next_draw

@@eight_block:                      ; The 4x4 block is split into 4 2x2 quadrants with 2 colors
    FILL_LINE_PAIR                  ; Fill first two lines
    add di,bp                       ; lpBuffer += wPitch
    FILL_LINE_PAIR                  ; Fill last two lines
    jmp @@col_loop_next_draw

@@fill_block:                       ; 1-color block, fill with a single color in AL
    mov ah,al                       ; Both bytes of AX now hold the fill color
    FILL_LINE                       ; Fill first line
    add di,bp                       ; lpBuffer += wPitch
    FILL_LINE                       ; Fill second line
    add di,bp                       ; lpBuffer += wPitch
    FILL_LINE                       ; Fill third line
    add di,bp                       ; lpBuffer += wPitch
    FILL_LINE                       ; Fill fourth line

@@col_loop_next_draw:               ; This is where we end up after a block where we drew something
    SUB_PITCH_THRICE                ; We're pointing to the start of the last line of the block, so backtrack
@@col_loop_next:
    add di,4                        ; Increment lpBuffer to point to next block (dl?)
    loop @@col_loop                 ; Advance to next column iteration
@@col_loop_done:
    pop dx                          ; Restore the number of blocks from left-to-right
    pop cx                          ; Restore the current row
    pop di                          ; Restore the current pointer to the destination

    add di,bp                       ; lpBuffer += wPitch * 4 (point to next row)
    add di,bp
    add di,bp
    add di,bp

    loop @@row_loop                 ; Advance to next row iteration

    ret
decode_msv1_8bit endp


; Changes needed for running in 32-bit code segment
; It should probably be enough to extend all uses of 16-bit registers
; (except when they represent data from memory) to 32-bit registers.

; Changes needed for 16-bit MSV1
; The 16-bit MSV1 format is mostly the same as the 8-bit format, with the
; major difference that colors are stored as 16-bit words rather than 8-bit
; bytes.