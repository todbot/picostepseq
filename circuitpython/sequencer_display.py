# sequencer_display.py -- picostepseq displayio object
# 9 Aug 2022 - @todbot / Tod Kurt
# Part of picostepseq : https://github.com/todbot/picostepseq/

import displayio
import terminalio
import vectorio

from adafruit_display_text import bitmap_label as label

uidebug = False
rotation = 0

# old horizontal layout
step_text_pos = ( (10,10), (40,10), (70,10), (100,10),
                  (10,30), (40,30), (70,30), (100,30) )
tempo_text_pos = (0, 57)
trans_text_pos = (50, 57)
seqno_text_pos = (0,45)
play_text_pos = (100, 57)
gate_text_offset = (0,-8)
gate_text_width, gate_text_height = (14,4)
edit_text_offset = (-5,0)

class StepSequencerDisplay(displayio.Group):
    def __init__(self, sequencer):
        super().__init__(x=0,y=0,scale=1)
        self.rotation = 0
        self.seq = sequencer
        #main = displayio.Group()
        gate_pal = displayio.Palette(1)
        gate_pal[0] = 0xffffff
        self.stepgroup = displayio.Group()
        self.editgroup = displayio.Group()
        self.gategroup = displayio.Group()
        self.append(self.stepgroup)
        self.append(self.editgroup)
        self.append(self.gategroup)
        font = terminalio.FONT
        for (x,y) in step_text_pos:
            self.stepgroup.append( label.Label(font, text="txt ", x=x, y=y, line_spacing=0.65))
            self.editgroup.append( label.Label(font, text="*", x=x+edit_text_offset[0], y=y+edit_text_offset[1]))
            self.gategroup.append( vectorio.Rectangle(pixel_shader=gate_pal,
                                                      width=gate_text_width, height=gate_text_height,
                                                      x=x+gate_text_offset[0], y=y+gate_text_offset[1]))

        self.tempo_text = label.Label(font, text="tmpo", x=tempo_text_pos[0], y=tempo_text_pos[1])
        self.play_text = label.Label(font, text="play", x=play_text_pos[0], y=play_text_pos[1])
        self.transpose_text = label.Label(font, text="trans", x=trans_text_pos[0], y=trans_text_pos[1])
        self.seqno_text = label.Label(font, text="seqno", x=seqno_text_pos[0], y=seqno_text_pos[1])
        self.append(self.tempo_text)
        self.append(self.play_text)
        self.append(self.transpose_text)
        self.append(self.seqno_text)

    def update_ui_step(self, step=None, n=0, v=127, gate=8, on=True, selected=False):
        if step is None:  # get current value
            step = self.seq.i
            n,v,gate,on = self.seq.steps[step]
        if uidebug: print("udpate_disp_step:", step,n,v,gate,on )
        notestr = self.seq.notenum_to_name(n)
        editstr = "." if selected else '*' if not on else ' '
        step_str = "%3s" % notestr
        if step_str != self.stepgroup[step].text:
            self.stepgroup[step].text = step_str
        self.editgroup[step].text = editstr
        self.gategroup[step].width = 1 + gate * gate_text_width // 16

    def update_ui_steps(self):
        for i in range(self.seq.step_count):
            (n,v,gate,on) = self.seq.steps[i]
            self.update_ui_step(i, n, v, gate, on)

    def update_ui_tempo(self):
        self.tempo_text.text = "bpm:%d" % self.seq.tempo

    def update_ui_playing(self):
        self.play_text.text = " >" if self.seq.playing else "||"

    def update_ui_transpose(self):
        self.transpose_text.text = "trs:%+2d" % self.seq.transpose

    def update_ui_seqno(self, msg=None):
        self.seqno_text.text = msg or f"seq: {self.seq.seqno}"

    def update_ui_all(self):
        self.update_ui_seqno()
        self.update_ui_tempo()
        self.update_ui_playing()
        self.update_ui_transpose()
        self.update_ui_steps()