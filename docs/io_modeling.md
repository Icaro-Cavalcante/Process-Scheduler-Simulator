# Input/Output (I/O) Modeling

---
- **Task:** ASH-02
- **Depends on:** `docs/modelo_processo.md` (ASH-01)
- **Acceptance criteria:** document addresses the 5 points required by Section 4 of the ASSIGNMENT.
- **Consumed by:** ELI-04 (implementation of the ready queue and I/O queue), and by the whole team —
this model is mandatorily the same for **all** algorithms evaluated (FCFS, Round Robin, Priority, and
the custom algorithm).

---

## 1. Purpose of this document

The ASSIGNMENT (Section 4) requires the team to explicitly define and document how I/O operations are
modeled, answering 5 mandatory questions. This document answers each of them and records why these
choices were made, so they can be cited and defended in the scientific article (JCK-13) and in the
presentation.

**Non-negotiable rule:** the modeling below is unique and must be used by all algorithms, in all
scenarios. No algorithm may have a different I/O queue, blocking time, or return behavior than the
others — that would invalidate the comparison required by the ASSIGNMENT.

---

## 2. When a process requests I/O

A process requests I/O **exactly when it finishes executing the current CPU burst and the next burst
in its `rajadas` list (see `docs/modelo_processo.md`) is of type `RAJADA_IO`**.

- There is no I/O request "in the middle" of a CPU burst — the simulation's granularity treats each CPU
  burst as indivisible from the I/O standpoint (it can be preempted by scheduling, but that does not
  trigger an I/O request).
- The moment the CPU burst ends, the simulation engine checks the next burst: if it is `RAJADA_IO`, the
  process is immediately moved from the **Running** state to **Blocked**
  (`docs/modelo_processo.md`, Section 5).

## 3. How long the process stays blocked

- The blocking time is exactly the `duracao` (duration) defined in the corresponding I/O burst
  (`Rajada.duracao`, randomly generated from the seed at workload-creation time — distribution
  parameters in `docs/cenarios.md`).
- This time is predictable, given the process and the seed: no additional variability is introduced by
  the simulation engine during execution (i.e., we do not model I/O contention that would extend the
  blocking time beyond the sampled value — see limitation in Section 7).
- The process remains in the **Blocked** state for this entire duration, counted from the instant it
  enters the I/O queue/device (see Section 5 on the queue).

## 4. Whether multiple I/O operations can occur in parallel

**Team decision: _NO_.**
> We adopt a single logical I/O device, shared by all processes, that serves **one request at a
time** (no parallelism).

- If more than one process requests I/O at the same simulation instant, they enter a device waiting
  queue (see Section 5) and are served in order.
- Justification: this choice simplifies the model and makes it easier to reproduce and debug, keeping
  the project's focus on **CPU scheduler** behavior (the object of study of the ASSIGNMENT), rather than
  on engineering a complex I/O subsystem with multiple disks/controllers.
- This simplification and its limitation are discussed in Section 7 below and should be revisited by
  `JCK-13` in the article.

## 5. Whether there is one or more I/O devices with their own queue

- There is **a single I/O device**, with **a single dedicated queue**, of the FIFO (First-In,
  First-Out) type: whoever requests I/O first is served first, regardless of the process's priority or
  the CPU scheduling algorithm in use.
- Important: the I/O queue **is not affected by the CPU scheduling policy**. A high-priority process
  does not "cut in line" for I/O — this keeps I/O as a subsystem that is neutral with respect to the
  experiment, isolating the effect we want to measure (the CPU scheduler).
- Suggested data structure: a simple queue (linked list or circular array) of PIDs, ordered by arrival
  time at the I/O queue.

`(FUTURE) Add a "warning" icon/image for better visualization`

## 6. When the process returns to the ready queue

- At the exact instant its blocking duration (Section 3) elapses and the device has already served it
  (i.e., it is no longer waiting in the device's queue, but rather being "served" for the duration of
  the burst), the process transitions from **Blocked** to **Ready**
  (`docs/modelo_processo.md`, Section 5).
- It enters the ready queue and goes back to competing for the processor under the same conditions as
  any other ready process, according to the policy of the algorithm in use (FCFS, RR, Priority, or the
  custom one).
- There is no "return priority": a process coming back from I/O receives no special treatment when
  entering the ready queue.

---

## 7. Discussion: how this modeling can influence the results

Points that should be revisited and expanded on by `JCK-13` in the article (this is only a suggested
starting point):

- **A single device with no parallelism** tends to create longer I/O queues in I/O-bound scenarios
  (many processes with many I/O bursts), which can inflate turnaround in a similar way across all
  algorithms — since the I/O queue is neutral to the CPU policy, the impact tends to be "shared" across
  algorithms, but it still needs to be observed in the results.
- By not modeling multiple devices, **we do not capture scenarios where real I/O parallelism would
  relieve the queue** — this is an intentional limitation, adopted to keep the experiment focused on CPU
  scheduling, and it should be cited as *future work* or a *limitation* in the article.
- Since the I/O queue is FIFO and neutral to priority, **Jain's fairness index of slowdown** can be
  affected by I/O waits that have no relation to the CPU scheduling policy being tested — this should be
  taken into account when interpreting the fairness metric (see Section 9 of the ASSIGNMENT — Expected
  Results).

---

## 8. Team review checklist

- The 5 points required by Section 4 of the assignment are answered explicitly and without ambiguity;
- ELI confirms that the ready queue + I/O queue (ELI-04) can be implemented directly from this
      document;
- Everyone agrees that this modeling will be identical for the 4 algorithms evaluated;
- The discussion in Section 7 has been read by JCK, who will use it as a basis for JCK-13 in the
      article.
