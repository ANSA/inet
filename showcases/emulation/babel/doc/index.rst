:orphan:

Testing manet routing protocols in a simulated environment
----------------------------------------------------------

want to say:

- this is about a real manet routing protocol running on the real computer
- that is put in a simulated environment
- so it easy to try it...try the real thing
- dont need sophisticated setup with moving nodes
- might not be as accurate as if you built it
- but it can be useful

.. INET's emulation features make it possible to test real-world routing protocol
   implementations in a simulated environment.

.. Testing routing protocols in realistic scenarios can be a complex endeavor.

Testing routing protocols with simulation is often easier than
building a test setup in the real world. INET's emulation features make it possible
to test a real-world routing protocol implementation (as opposed to a simulation model)
in a simulated environment. A simulation model of the protocol is not required, as the
simulated environment and multiple instances of the real-world implementation and can
run on the same host computer.

This showcase demonstrate the testing of the Babel manet routing protocol.
Babel's linux implementation is used. The protocol is real, but the position and movement
of nodes using it is simulated.

In this showcase, we'll demonstrate such a test scenario with the Linux implementation of the Babel MANET routing protocol. We'll use multiple instances of the protocol; the position and movement of the nodes using it will be simulated.
