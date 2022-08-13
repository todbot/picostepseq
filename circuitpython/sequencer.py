# sequencer.py -- picostepseq sequencer object
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

note_names = ("C","C#","D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")

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
    def __init__(self, step_count, tempo, on_func, off_func, playing=False, seqno=0):
        self.steps_per_beat = 4  # 16th note
        self.step_count = step_count
        #self.last_step = last_step   #  || step_count
        self.i = 0  # where in the sequence we currently are
        self.steps = [ (0,0,8,True) ] * step_count  # step "object" is tuple (note, vel, gate, on)
        self.on_func = on_func    # callback to invoke when 'note on' should be sent
        self.off_func = off_func  # callback to invoke when 'note off' should be sent
        self.set_tempo(tempo)
        self.last_beat_millis = ticks_ms()  # 'tempo' in our native tongue
        self.held_gate_millis = 0  # when in the future our note off should occur
        self.held_note = (0,0,0,0)  # the current note being on, to be turned off
        self.transpose = 0
        self.playing = playing   # is sequence running or not (but use .play()/.pause())
        self.seqno = seqno # an 'id' of what sequence it's currently playing

    def set_tempo(self,tempo):
        self.tempo = tempo
        self.beat_millis = 60_000 // self.steps_per_beat // tempo
        print("seq.set_tempo: %6.2f %d" % (self.beat_millis, tempo) )

    def update(self):
        now = ticks_ms()
        delta_t = now - self.last_beat_millis
        if self.playing and delta_t > self.beat_millis:
            self.i = (self.i + 1) % self.step_count
            (note,vel,gate,on) = self.steps[self.i]  # get new note
            note += self.transpose
            if self.held_gate_millis > 0:  # turn off pending note
                print("HELD NOTE", self.notenum_to_name(self.held_note[0]), self.held_note[2],
                      now, self.held_gate_millis, delta_t, self.beat_millis)
                self.off_func( *self.held_note )  # FIXME: why is this getting held?
            self.on_func(note, vel, gate, on)
            err_t = delta_t - self.beat_millis  # how much we are over
            self.last_beat_millis = now - err_t  # adjust for our overage
            self.held_note = (note,vel,gate,on) # save for note off later
            self.held_gate_millis = now - err_t + ((self.beat_millis * gate) // 16) # gate ranges from 1-16

        # after gate, turn off note
        if self.held_gate_millis != 0 and now > self.held_gate_millis:
            self.held_gate_millis = 0
            self.off_func( *self.held_note )

    def toggle_play_pause(self):
        if self.playing:
            self.pause()
        else:
            self.play()

    def stop(self):  # FIXME: what about pending note
        self.playing = False
        self.i = 0
        self.last_beat_millis = 0

    def pause(self):
        self.playing = False

    def play(self, play=True):
        self.last_beat_millis = ticks_ms() - self.beat_millis # ensures we start on immediately
        self.playing = True

    # return note and octave as string,int
    def notenum_to_noteoct(self, notenum):
        octave = notenum // 12 - 1;
        notename = note_names[notenum % 12]
        return (notename, octave)

    # old
    def notenum_to_name(self, notenum, separator=""):
        octave = notenum // 12 - 1;
        n = notenum % 12
        return note_names[n] +separator+ str(octave)
