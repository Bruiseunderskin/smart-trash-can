# .agents/skills

One directory per skill. Each skill holds a `SKILL.md` with YAML frontmatter
(`name`, `description`) followed by the instructions an agent loads on demand.

| Skill | Use when |
|---|---|
| `embedded-ui-visual-quality/` | reviewing or improving an LVGL screen on the 4.3″ panel — typography, contrast, alignment, motion |

Rules:

- A skill states **what to do and why**, not project history.
- Numbers that matter (pixel sizes, contrast ratios, timings) belong in the
  skill; one-off measurements belong in `docs/`.
- Keep each `SKILL.md` short enough to load whole; push long reference material
  into a `references/` subdirectory.
