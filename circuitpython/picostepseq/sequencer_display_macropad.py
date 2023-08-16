# sequencer_display_macropad.py -- picostepseq displayio object for MacroPad RP2040
# 9 Aug 2022 - @todbot / Tod Kurt
# Part of picostepseq : https://github.com/todbot/picostepseq/

import displayio
import terminalio
import vectorio

from adafruit_display_text import bitmap_label as label
from adafruit_bitmap_font import bitmap_font

from sequencer_display import SequencerDisplay

uidebug = False

# vertical layout for MacroPadRP2040
step_text_pos = ( (0,10), (16,10), (32,10), (48,10),
                  (0,50), (16,50), (32,50), (48,50)  )
bpm_text_pos = (0, 115)
bpm_val_pos = (25, 115)
trans_text_pos = (0, 100)
trans_val_pos = (25, 100)
seqno_text_pos = (0,85)
play_text_pos = (50,115)
oct_text_offset = (2,11)  # four per line
gate_bar_offset = (1,-8)
gate_bar_width, gate_bar_height = (14,4)
edit_text_offset = (3,20)

class SequencerDisplayMacroPad(SequencerDisplay):
    def __init__(self, sequencer):
        super().__init__(sequencer)

    def setup(self): # called by superclass init
        gate_pal = displayio.Palette(1)
        gate_pal[0] = 0xffffff
        self.notegroup = displayio.Group()
        self.octgroup = displayio.Group()
        self.editgroup = displayio.Group()
        self.gategroup = displayio.Group()
        self.append(self.notegroup)
        self.append(self.octgroup)
        self.append(self.editgroup)
        self.append(self.gategroup)
        font = bitmap_font.load_font("helvB12.pcf")
        #font = terminalio.FONT
        font2 = terminalio.FONT
        for (x,y) in step_text_pos:
            self.notegroup.append( label.Label(font, text="A#", x=x, y=y, line_spacing=0.65))
            self.octgroup.append( label.Label(font2, text="0", x=x+oct_text_offset[0], y=y+oct_text_offset[1]))
            self.editgroup.append( label.Label(font2, text="*", x=x+edit_text_offset[0], y=y+edit_text_offset[1]))
            self.gategroup.append( vectorio.Rectangle(pixel_shader=gate_pal,
                                                      width=gate_bar_width, height=gate_bar_height,
                                                      x=x+gate_bar_offset[0], y=y+gate_bar_offset[1]))

        self.seqno_text = label.Label(font2, text="seqno", x=seqno_text_pos[0], y=seqno_text_pos[1])
        self.bpm_text = label.Label(font2, text="bpm:", x=bpm_text_pos[0], y=bpm_text_pos[1])
        self.bpm_val = label.Label(font2, text="bpm:", x=bpm_val_pos[0], y=bpm_val_pos[1])
        self.transpose_text = label.Label(font2, text="trans", x=trans_text_pos[0], y=trans_text_pos[1])
        self.transpose_val = label.Label(font2, text="+0", x=trans_val_pos[0], y=trans_val_pos[1])
        self.play_text = label.Label(font, text="||", x=play_text_pos[0], y=play_text_pos[1])
        self.append(self.bpm_text)
        self.append(self.bpm_val)
        self.append(self.play_text)
        self.append(self.transpose_text)
        self.append(self.transpose_val)
        self.append(self.seqno_text)

    def update_ui_step(self, step=None, n=0, v=127, gate=8, on=True, selected=False):
        if step is None:  # get current value
            step = self.seq.i
            n,v,gate,on = self.seq.steps[step]
        if uidebug: print("udpate_disp_step:", step,n,v,gate,on )
        (notename,octave) = self.seq.notenum_to_noteoct(n)
        notestr = notename
        octstr = str(octave)
        editstr = "^" if selected else '*' if not on else ' '
        if notestr != self.notegroup[step].text:
            self.notegroup[step].text = notestr
        if octstr != self.octgroup[step].text:
            self.octgroup[step].text = octstr
        self.editgroup[step].text = editstr
        self.gategroup[step].width = 1 + gate * gate_bar_width // 16
