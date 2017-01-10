% Strace.eBPF
% **Fast syscall's tracing**
% Vitalii Chernookyi

******

Why we need new tool
---------------------

 - regular system tracing tools are slow
 - regular tools slowdown traced application for few orders
 - output of regular tools is human-oriented and don't assume automated
   processing
 - overcoming above problems in regular way require:

   - kernel hacking (sysdig)
   - special HW (Lauterbach).

******

Used technologies
------------------

 - eBPF
 - KProbe
 - Perf Event Circular Buffer
 - event-loop

******

System requirements
--------------------

 - libbcc
 - Linux Kernel 4.4 (for Perf Event Circular Buffer)
 - CAP_SYS_ADMIN capability for bpf() syscall
 - mounted tracefs

******

Pros
-----

 - Used combination of technologies allow tool to be about one order faster
   than regular system strace.
 - This tool consume much less amount of CPU resource
 - Output of this tool is designed to be suitable for processing with
   classical tools and technologies, like awk.
 - Could trace syscalls system-wide.
 - Could trace init (process with 'pid == 1'). Finally we have a proper
   tool for debugging systemd ;-)

******

Cons
-----

 - Limited functionality
 - Slow attaching and detaching
 - Asynchronity. If user will not provide enough system resources for
   performance tool will skip some calls. Tool does not assume to try
   any work-around behind the scene.
 - Depend on modern kernel features
 - Limited possibility to run few instances simultaneously.
 - Underlaing eBPF technology still is in active development. So we should
   expect hangs and crashes more often as for regular strace, especially on
   low-res systems.

******

Structural Component Diagram
-----------------------------

![DSGN_struct_comp_dia.png](DSGN_struct_comp_dia.png)

******

Behavioral Activity Diagram
----------------------------

![DSGN_beh_act_dia.png](DSGN_beh_act_dia.png)

******

Conclusion
-----------

 - we reached performance about 1000000 syscalls per second.
 - there is places for future optimization.
