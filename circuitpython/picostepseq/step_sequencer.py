
class Step:
    def __init__(self, note, vel):
        self.note = note
        self.vel = vel   
    
# 120 bpm = 120 beats / 60 seconds
# beat_secs = 60 / temo, beat_millis = 60000 / tempo

class StepSequencer:
    def __init__(self, step_count, tempo, playfunc):  # also send callbacks?
        self.step_count = step_count
        #self.last_step = last_step   #  || step_count
        self.tempo = tempo
        self.beat_secs = 60 / self.tempo
        self.last_beat_time = time.monotonic()
        self.i = 0
        self.steps = [ (0,0) ] * step_count
        self.playfunc = playfunc
        self.playing = True
        print("beat_secs:", self.beat_secs, self.tempo)
        #self.steps = []
        #for i in range(step_count):
        #    self.steps.append( Step(30 + i,10*i) )

    def set_tempo(self,t):
        self.tempo = t
        self.step_time = 0
        
    def update(self):
        #if not self.playing: return
        if time.monotonic() - self.last_beat_time > self.beat_secs:
            self.last_beat_time = time.monotonic()
            self.i = (self.i + 1) % self.step_count
            (note,vel) = self.steps[self.i]
            self.playfunc(self.i, note, vel)
            #print("beat %d t:%8d n:%3d v:%3d" %
            #      (self.i, int(time.monotonic()*1000), note, vel ))

    def set(self, step, note, vel=127):
        print("set:",step,note,vel)
        self.steps[step] = (note,vel)
        
    def play(self):
        self.playing = True
        
    def pause(self):
        self.playing = False
    


if __name__ == "__main__":
    import time, random
    
    print("hello there")

    def play_note(step, note, vel):
        print("play: s:%d n:%3d v:%3d" % (step, note,vel) )
        
    seq = StepSequencer(8, 120, play_note)

    last_time = time.monotonic()
    while True:
        seq.update()
        
        if time.monotonic() - last_time > 5:
            last_time = time.monotonic()
            print("hi")
            i = random.randint(0,8)
            n = random.randint(30,50)
            v = random.randint(30,120)
            seq.set( i, n, v)
            
        
