# GGPOUE

A port of [GGPO](http://ggpo.net) to an Unreal Engine plugin.

## Setup & Usage

Add to the Plugins folder of your Unreal project.

See [doc/README.md](doc/README.md), [doc/DeveloperGuide.md](doc/DeveloperGuide.md), and the [GGPO GitHub](https://github.com/pond3r/ggpo) for more information.

### Sample Application

[VectorWar UE](https://github.com/BwdYeti/VectorWarUE) is a port of the GGPO sample game VectorWar, using GGPOUE for netcode.

### Issues

Currently only usable with Windows, as the GGPO source and network layer depend on Win32 APIs. May be able to reuse some UE functionality for the underlying connection?

## Licensing

GGPO is available under The MIT License. This means GGPO is free for commercial and non-commercial use. Attribution is not required, but appreciated. 

## External packet transports

This branch adapts the transport architecture from the MIT-licensed
[erebuswolf network-abstraction fork](https://github.com/erebuswolf/ggpo-Unreal-Plugin-with-Network-Abstraction),
while retaining the current GGPOUE module, Unreal APIs and protocol hardening.
See LICENSE for GroundStorm, BwdYeti and Friendly Fish Games attribution.

The existing `ggpo_start_session(..., localport)` API still creates a UDP backend.
The new `ggpo_start_session(session, callbacks, manager, game, players, input_size)`
overload borrows a `ConnectionManager`; keep it alive until `ggpo_close_session`.
External remote players set `GGPOPlayer::connection_id` to a registered ID.
An equivalent spectator overload accepts the host's connection ID.
Connection IDs belong to the manager/session, independently of participant slots.

Implement nonblocking, message-preserving send/receive and registered-peer lookup.
Receive returns a positive byte count for a valid datagram, -1 when drained or on
an error, and -2 when an invalid datagram was consumed. Empty UDP packets are
ignored; GGPO still owns protocol disconnect and timeout behavior. The packet
pump validates lengths/IDs and bounds work to 256 datagrams per poll.

This is a transport boundary, not a supplied Steam backend. Advanced Sessions
participant discovery and Steam packet delivery must be integrated explicitly.
The merge intentionally retains the current packet validation, compatibility
handshake, callback failure halting, confirmation diagnostics and SLS capacities;
it does not import the old fork's unrelated UE4/platform/protocol changes.

Merge validation (UE 5.8, 2026-10-07): editor build succeeded; all 32
RollbackCombat automation tests passed, including external transport routing;
an impaired two-process native Box3D/UDP match matched all 1,200 confirmed frame
hashes and the previous reference final hash `ef8d6f3f`, with zero desyncs.
Four-slot external session creation is covered by a unit test; four-player
gameplay, Steam delivery and host migration are not established by these checks.
