# sequencer_display.py -- picostepseq displayio object for MacroPad RP2040
# 6 Aug 2022 - @todbot / Tod Kurt
# Part of picostepseq : https://github.com/todbot/picostepseq/

# let's get a millis function
try:
     from supervisor import ticks_ms  # thank you dhalbert
except (ImportError,NameError,NotImplementedError):
     from time import monotonic_ns as _monotonic_ns  # assume monotonic_ns() exists else we are lame
     def ticks_ms(): return _monotonic_ns() // 1_000_000  # stolen from adafruit_ticks

def ticks_diff(t1,t2): return t1-t2

###gate_default = 8  # == 50%  (ranges 0-15)

# class Step:
#     note = 0
#     vel = 0
#     gate = 1  # ranges from 1-16
#     on = True
#     # def __init__(self, note,vel=127,on=True):
#     #     self.note = note
#     #     self.vel = vel
#     #     self.gate = 0-1? or 0-15?
#     #     self.on = on

class StepSequencer:
    def __init__(self, step_count, tempo, on_func, off_func):
        self.steps_per_beat = 4  # 16th note
        self.step_count = step_count
        #self.last_step = last_step   #  || step_count
        self.i = 0
        self.steps = [ (0,0,8,True) ] * step_count  # step "object" is tuple (note, vel, gate, on)
        self.on_func = on_func
        self.off_func = off_func
        self.set_tempo(tempo)
        self.last_beat_millis = ticks_ms()
        self.next_gate_millis = 0
        self.prev_note = (0,0)
        self.transpose = 0
        self.playing = True

    def set_tempo(self,tempo):
        self.tempo = tempo
        self.beat_millis = 60_000 // self.steps_per_beat // tempo
        print("seq.set_tempo: %6.2f %d" % (self.beat_millis, tempo) )

    def update(self):
        now = ticks_ms()
        delta_t = now - self.last_beat_millis
        if self.playing and delta_t > self.beat_millis:
            self.i = (self.i + 1) % self.step_count
            (note,vel,gate,on) = self.steps[self.i]
            note += self.transpose
            self.next_gate_millis = now + ((self.beat_millis * gate) // 16)  # gate ranges from 1-16
            self.prev_note = (note,vel,gate,on) # save for note off
            self.on_func(note, vel, gate, on)
            err_t = delta_t - self.beat_millis  # how much we are over
            self.last_beat_millis = now - err_t  # adjust for our overage

        # FIXME: this is broken => stuck notes when params are changed
        if now > self.next_gate_millis and self.next_gate_millis != 0:
            self.next_gate_millis = 0
            (onote, ovel, ogate, oon) = self.prev_note
            (note, vel, gate, on) = self.steps[self.i]  # just for 'on' value
            self.off_func(onote, ovel, ogate, on)

    def stop(self):  # FIXME: what about pending note
        print("stop!")
        self.playing = False
        self.i = 0
        self.last_beat_millis = 0

    def pause(self):
        self.playing = False
        pass

    def play(self, play=True):
        pass

    def notenum_to_name(self,notenum):
        octave = notenum // 12 - 1;
        n = notenum % 12
        note_names = ("C ","C#","D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B ")
        return note_names[n] +"\n"+ str(octave)



# if __name__ == "__main__":
#     import time, random

#     print("hello there")

#     def play_note(step, note, vel):
#         print("play: s:%d n:%3d v:%3d" % (step, note,vel) )

#     seq = StepSequencer(8, 120, play_note)

#     last_time = time.monotonic()
#     while True:
#         seq.update()

#         if time.monotonic() - last_time > 5:
#             last_time = time.monotonic()
#             print("hi")
#             ri = random.randint(0,8)
#             rn = random.randint(30,50)
#             rv = random.randint(30,120)
#             seq[ri] = (rn, rv)
