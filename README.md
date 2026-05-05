# Zoltan's modified Flipper Zero app collection

This is my Flipper Zero application repository. If you know how to program, you will probably be appalled, because some of this code is AI-generated, or at least AI-assisted.

In particular:

* [flipper-pocsag-pager](flipper-pocsag-pager/)
Dive back to the 1980s and 1990s with your very own pager.
* [flipper-gui-catalogue](flipper-gui-catalogue)
This app is not really useful. It literally does nothing besides looking pretty.
* [avs-2026-demo](avs-2026-demo)
An application to demonstrate characteristic colour shift with RGB LEDs at an applied vision science conference. Needs some external hardware, but it's easy to make.

I started this repository to test my own AI-based code generation pipeline: it's offline, has some custom tools with something else that I call the 'thought process'. I don't think I have enough to publish this latter part yet, I am on a ride myself with this as I write it.

## In general: Why

Because AI generated code is nothing but a higher-level abstraction of _implementing what you want_, a generational shift in the same way how compiled programming languages were invented in the 1970s and replaced direct-to-machine-code programming that was commonplace in the 1960s (think punchcards, teletype paper tapes, etc.).

Before you judge, hear me out:

Let's say, you want to write code to execute a task repeatedly. Let's say that the task is ~~stabbing yourself in the face~~ to do nothing for a hundred times.

In PC assembly, this is easy:

```Assembly
section .text
global _start

_start:
    mov ecx, 100      ; loop counter
loop_start:v
    dec ecx           ; ecx = ecx - 1
    jnz loop_start    ; jump if ecx != 0

    ; exit (Linux syscall)
    mov eax, 1
    xor ebx, ebx ; This just deletes the content of the register
    int 0x80 ;DOS would be int 0x21

```

...and this is the binary code that runs on your computer (IF it runs, at all...)

```
B9 64 00 00 00
49
75 FB
B8 01 00 00 00
31 DB
CD 80
```

In MS-DOS, there is some overhead - let's say we program in C, because we are no longer running on bare silicon and we have an OS that takes care about the hardware, and software interrupts, executable signatures `MZ` and the likes:

```C
void main() {
    int i;
    for (i = 0; i < 100; i++) {
        // do nothing here
    }
}
```

```Assembly
4D 5A 1A 00 01 00 00 00 00 00 00 00 FF FF 00 00
B9 64 00 E2 FE B8 00 4C CD 21 00 00 00 00 00 00

... which roughly translates to:

0010: B9 64 00        mov cx, 100
0013: E2 FE           loop 0013h - 2 = 0011h
0015: B8 00 4C        mov ax, 4C00h
0018: CD 21           int 21h

```

Going one level above, this is what is running on the machine when a loop that does nothing is written in C#:

```CSharp
using System;

class Program
{
    static void Main()
    {
        for (int i = 0; i < 100; i++)
        {
            // do nothing here
        }
    }
```

...which then results in something like:

```
.locals init (int32 V_0)
IL_0000: ldc.i4.0
IL_0001: stloc.0
IL_0002: br.s IL_000A

IL_0004: ldloc.0
IL_0005: ldc.i4.1
IL_0006: add
IL_0007: stloc.0

IL_0008: nop

IL_000A: ldloc.0
IL_000B: ldc.i4.s 100
IL_000D: blt.s IL_0004

IL_000F: ret


4D 5A 50 00 01 00 00 00 04 00 00 00 FF FF 00 00
B8 00 00 00 00 00 00 00 40 00 00 00 00 00 00 00
00 00 00 00 40 00 00 00 00 00 00 00 00 00 00 00
50 45 00 00 4C 01 01 00 00 00 00 00 00 00 00 00
E0 00 0F 01 0B 01 02 00 00 10 00 00 00 10 00 00
00 00 00 00 00 20 00 00 00 02 00 00 00 02 00 00
00 00 00 00 00 00 00 00 B9 64 00 E2 FE 31 C0 C3
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

...which, when stripping all the dependencies and other ever-so-Microsoft-related stuff, roughly translates to:

```
; Setup i = 0
xor     eax, eax            ; Set EAX register to 0
jmp     SHORT G_M000_IG04   ; Jump down to the loop condition

G_M000_IG03:
; i++
inc     eax                 ; Increment EAX by 1

G_M000_IG04:
; Compare i to 100
cmp     eax, 100            ; Compare EAX to 100 (0x64 in hex)
jl      SHORT G_M000_IG03   ; Jump to G_M000_IG03 if less than

; Exit
ret                         ; Return from method
```

...which is kind of similar to the Assembly code, but with much more overhead. And I hope you will forgive me for not creating register-analogue examples here.

I think you can see that the higher level of abstraction you go, the more inefficient you get, because more instructions have to be executed on silicon to achieve the same thing. Timing-wise, we don't care, computers are fast these days. If your computer is slow, it's probably due to performance bottlenecks in networking, storage and hardware interconnect buses. This has escalated to the point where a lot of people do not work in _compiled_ languages at all, but _interpreted_ ones (Python, Matlab, R), where the computational overheads can be several order of magnitudes larger, and still get some decent performance.

Creating code with generative AI is just an implementation method of an even higher, more human-friendly, _semantic_ level, in exchange for some inefficiencies on a _modal_ level. Yes, I used grammatical jargon here, because the current state of the art is using language models, which, by principle, work with text, and flow of patterns within the text. In other words, the generated code will be ugly.

But unlike normal AI generated text from a chatbot, writing code is far more deterministic and rigid. And, because the generated code can easily be verified for functionality (Does it compile? Does it do what I originally intended to do?), it is easier for us humans to achieve the _semantic goals_ that we set. With the use of an agent, you can even automate the process of code generation and testing too.

So, to keep this example of a loop doing nothing a hundred times, a language model will have no problem creating something like this:

```C
do {
    int i = 0;
    for(i=0; i<100; i++) {
        // do nothing here
    }
} while (0)
```

...which, most C-programmers would say that the above implementation is outright idiotic, but will agree it is still, _semantically_ correct. From a user's point of view, this code still does nothing a hundred times, exactly as intended.

And therefore, there is no point about discussing this further - people can refer to this derogatorily as vibe coding or spewing AI slop, but unless there is something very specific, such as high-security stuff, life-support stuff, kernel development, or some design corners must be avoided, it will do. Just as how the overheads from higher-level programming languages will do. Ultimately, _you_ are responsible for the _semantics_ alone, i.e. you must make sure that the code is running correctly and satisfies the requirements.

### Why with the Flipper Zero

I wanted to learn how to write applications for it. To me, it turned out this process to be incredibly labour-intensive, the code becomes very complicated very quickly, even for simple things. There is minimal documentation which is up to date, and while [Derek Jamison's tutorials](https://github.com/jamisonderek/flipper-zero-tutorials/) are great, they only take you so far. Code from other users and people are iffy at best. I found some quite horrible stuff that was hard-coded to be dysfunctional. While I went on my own, very quickly I found that I get some really cryptic error messages, and I found that not only I have to be familiar with the API, but the entire Flipper firmware too. It's OK, it's mostly open-source: bugs, unimplemented features and breaking changes from version to version are normal. Me being ambitious, I did buy a [Flipper clone with half the hardware missing and/or inferior](https://github.com/ha5dzs/geekzero-firmware), got into (admittedly a rather small degree of) understanding the firmware, only to get to the cathartical realisation that I will never be able to fully understand the entirety of it due to its sheer volume. I just don't have the time to comprehend it all, from high-level API to register-level on the hardware itself. And I am speaking of experience, I [wrote the MOTOM toolbox for a motion tracker](https://github.com/ha5dzs/motom-toolbox) that literally took 18 months away from my life understanding how it works and implementing basic sanity checks that were not included at all.

When you go down this rabbit hole, there is really no turning back without suffering from PTSD. I am an interdisciplinary scientist, people not understanding what I do is everyday occurrence to me, I am used to being scientifically alone. But figuring this out is such as subproblem of a subproblem of a subproblem that I decided to take another approach and explore something new, with AI.

![the meme](img/callback_function_meme.jpeg)

All it took is some armed conflict in my area, a mandate from working from home, and upgrading my already-decent workstation that I could borrow from work. As the code is mostly open-source, I can see both the API the developers get and what it does on the other side inside the device firmware. So it's much easier for me to determine whether something crashes due to me not doing something right, or me trying to breathe life into something that is supposed to work theoretically but practically it doesn't.

Anyway, see the sub-directories here, each of them are dedicated projects at various complexity.
