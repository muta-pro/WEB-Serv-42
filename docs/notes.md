I also asked about the HEAD method and Ive decided I would like to handle it, corresponding context:
HEAD.

Per RFC 9110 §9.3.2, HEAD is defined as identical to GET except the server MUST NOT return a message body. Because of that coupling, any server that supports GET is expected to support HEAD — and nginx enforces this in limit_except: restricting GET automatically restricts HEAD too. It's not a separate permission decision.

This ties directly into your outstanding Phase 2 item on bodyless responses (HEAD/204/304 in toBytes()). Worth thinking about where HEAD-handling lives in your pipeline:

In the config? If allowedMethods contains GET, do you auto-insert HEAD at parse time so the router's method check just works? Or do you special-case HEAD in the router (if method == HEAD, treat as GET for permission check)?
In the response builder? Either way, the actual body suppression is a toBytes() concern — build the response as if it were a GET (including Content-Length), then drop the body bytes on the way out. Do not recompute Content-Length as zero — it must reflect what the body would have been. That's the classic HEAD trap.

The subject only lists GET/POST/DELETE as required, and doesn't mention HEAD at all — so an evaluator can't fail you for skipping it. But browsers and tools (including curl -I) do send HEAD, and getting a 405 back looks broken. Cheap to get right.

Related bodyless cases to keep in the same mental bucket while you're auditing toBytes(): 204 No Content (MUST NOT include a body, MUST NOT include Content-Length or Transfer-Encoding), 304 Not Modified (same body rule, but Content-Length behavior differs — must match what a 200 would have sent). RFC 9110 §6.4.1 and §15.4.5 are the references.

Quick question: does your allowedMethods set live as std::set<std::string> (per your r-p0-2 sketch) or as a bitmask/enum? If string set, "GET" and "HEAD" auto-inclusion is a two-line insert at parse time. If bitmask, even easier — a single OR.

---VAULT SYNC---
date: 2026-09-02
note: projects/webserv/index.md

What happened this session
Confirmed HEAD is the method auto-granted with GET per RFC 9110 §9.3.2; nginx limit_except enforces the same coupling.
Connected HEAD handling to the outstanding Phase 2 bodyless-response audit; flagged the "don't zero out Content-Length" trap.
Raised design choice: auto-insert HEAD into allowedMethods at parse time vs special-case in the router.
Subject doesn't require HEAD, but skipping it breaks common tools (curl -I, browsers) — cheap to include.
Updated next actions
Finalize ServerConfig fields per subject §IV.3 plus deliberate defaults.
Finalize LocationConfig fields — add missing upload-storage-location field.
Decide: alias-style vs root-style directory mapping for the /kapouet example; document exact string-op rule.
Decide: inheritance at parse-time vs lookup-time; document.
Decide: keep or drop serverName; if drop, confirm listen-socket → ServerConfig mapping is unambiguous.
Decide error-page storage semantics — URI (nginx-faithful) vs filesystem path (simplified).
Decide config micro-syntax: delimiters, comment style, size suffixes, multi-code error_page, no-match fallback behavior.
Write 2–3 example config files by hand (minimal, realistic, adversarial) as parser test corpus.
New: decide HEAD-handling location — auto-insert into allowedMethods at parse time, or special-case in router (HEAD → treat as GET for permission check).
Bring alias-vs-root rule + Host-header handling to Liza's Meeting 2 as interface contract items.
Continue Phase 2 pending items: response code audit; bodyless-response rules (HEAD/204/304) in toBytes() — remember 204 forbids body + Content-Length; 304 forbids body but keeps Content-Length as-if-200; HEAD forbids body but keeps Content-Length.
Anything to add to key context
RFC 9110 §9.3.2 (HEAD), §6.4.1 (Content-Length semantics), §15.4.5 (304 Not Modified) — the three refs for the bodyless-response audit.
HEAD-body suppression rule: build the full response, drop body bytes at wire-send time. Never recompute Content-Length as zero.
---END SYNC---
