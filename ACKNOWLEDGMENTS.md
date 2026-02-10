# Acknowledgements

This library started as a simple idea: I wanted a fluent API for ISOBUS so application code reads like intent instead of a pile of PGN constants.

Then it was “just one more thing”. Then another. Then a rewrite. And now… another rewrite.

This time around, the refactor has been heavily assisted by AI. It’s been equal parts accelerator and rubber-duck therapist.

It didn’t come out of a vacuum though. A lot of the “how should this behave on a real bus?” work is learned the hard way by other projects first.

Honestly: without existing stacks to learn from, I wouldn’t have gotten anything working at all.

And, yes, a big chunk of what’s here is effectively “a copy with a nicer API” - a very subjective, convenience-focused API - wrapped around the same hard protocol truths.

Credit where it’s due:

- **[AgIsoStack++](https://github.com/Open-Agriculture/AgIsoStack-plus-plus)** - A very practical, battle-tested ISO11783/J1939 stack. We keep a vendored copy under `xtra/AgIsoStack-plus-plus/` and use it as a reference for protocol state machines, edge cases, and interoperability behavior.

  - Upstream: https://github.com/Open-Agriculture/AgIsoStack-plus-plus
  - Docs: https://agisostack-plus-plus.readthedocs.io/en/latest/index.html
  - License: MIT (see `xtra/AgIsoStack-plus-plus/LICENSE`)
