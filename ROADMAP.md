# TerminalDX12 Roadmap

**Last Updated**: 2025-12-28
**Issue Tracker**: [GitHub Issues](https://github.com/maxlawrie/TerminalDX12/issues)

## Summary

| Phase | Status | Tasks | Remaining |
|-------|--------|-------|-----------|
| 1. Core VT Compatibility | Complete | 84 | 0 |
| 4. Advanced Features | In Progress | 43 | 43 |
| 5. Polish & Refinement | Not Started | 75 | 75 |
| **Total** | | **202** | **118** |

---

## Bugs

| Issue | Description | Priority |
|-------|-------------|----------|
| [#2](https://github.com/maxlawrie/TerminalDX12/issues/2) | Alternate buffer resize does not preserve content | - |

---

## Completed: Phase 1 - Core VT Compatibility

All 84 tasks complete. Features delivered:
- True color (24-bit RGB) support
- Dim text rendering (SGR 2)
- Cursor save/restore (DECSC/DECRC)
- Application cursor keys (DECCKM)
- Auto-wrap mode (DECAWM)
- Cursor visibility (DECTCEM)
- Scroll regions (DECSTBM)
- Device status reports (DSR)
- Strikethrough text
- Bracketed paste mode
- Character/line operations (ICH, DCH, IL, DL, ECH, CHA)

---

## Phase 4: Advanced Features

| Issue | Feature | Tasks | Priority |
|-------|---------|-------|----------|
| [#4](https://github.com/maxlawrie/TerminalDX12/issues/4) | Enhanced Search (regex + scrollback) | 12 | P2 |
| [#6](https://github.com/maxlawrie/TerminalDX12/issues/6) | Tab Persistence & Reordering | 15 | P3 |
| [#10](https://github.com/maxlawrie/TerminalDX12/issues/10) | Search All Tabs | 6 | P3 |
| [#9](https://github.com/maxlawrie/TerminalDX12/issues/9) | URL Context Menu | 5 | P3 |
| - | Phase 4 Polish & Docs | 5 | - |

---

## Phase 5: Polish & Refinement

| Issue | Feature | Tasks | Priority |
|-------|---------|-------|----------|
| [#3](https://github.com/maxlawrie/TerminalDX12/issues/3) | Settings Dialog (GUI configuration) | 25 | P1 |
| [#5](https://github.com/maxlawrie/TerminalDX12/issues/5) | Font Ligatures (HarfBuzz) | 16 | P2 |
| [#7](https://github.com/maxlawrie/TerminalDX12/issues/7) | Window Transparency | 8 | P3 |
| [#8](https://github.com/maxlawrie/TerminalDX12/issues/8) | Window Modes (always-on-top, borderless) | 5 | P3 |
| [#11](https://github.com/maxlawrie/TerminalDX12/issues/11) | Multi-Window Support | 15 | P3 |
| - | Phase 5 Polish & Docs | 6 | - |

---

## Tech Debt

| Issue | Description |
|-------|-------------|
| [#12](https://github.com/maxlawrie/TerminalDX12/issues/12) | Address code TODOs |

---

## Recommended Execution Order

### Immediate (High Value)

1. [#3](https://github.com/maxlawrie/TerminalDX12/issues/3) **Settings Dialog** (P1) - Most requested user feature
2. [#4](https://github.com/maxlawrie/TerminalDX12/issues/4) **Enhanced Search** (P2) - High utility for power users

### Short Term

3. [#5](https://github.com/maxlawrie/TerminalDX12/issues/5) **Font Ligatures** (P2) - Developer experience improvement
4. [#7](https://github.com/maxlawrie/TerminalDX12/issues/7) **Window Transparency** (P3) - Quick win, low complexity

### Medium Term

5. [#6](https://github.com/maxlawrie/TerminalDX12/issues/6) **Tab Persistence** (P3) - Quality of life improvement
6. [#9](https://github.com/maxlawrie/TerminalDX12/issues/9) **URL Context Menu** (P3) - Small but useful feature
7. [#8](https://github.com/maxlawrie/TerminalDX12/issues/8) **Window Modes** (P3) - Power user features

### Long Term (Optional)

8. [#11](https://github.com/maxlawrie/TerminalDX12/issues/11) **Multi-Window Support** (P3) - Major refactor, tabs may suffice
9. [#10](https://github.com/maxlawrie/TerminalDX12/issues/10) **Search All Tabs** (P3) - Nice to have

---

## Version History

- **v0.1.0** - Initial release with core terminal functionality
- **Next** - Phase 4 (Advanced Features) + Phase 5 (Polish)
