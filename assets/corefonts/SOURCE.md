# Times New Roman — original Core Fonts for the Web package

- Original unmodified package: `times32.exe` (661,728 bytes).
- Source: https://sourceforge.net/projects/corefonts/files/the%20fonts/final/times32.exe/
- Project and redistribution notes: https://corefonts.sourceforge.net/
- Original accompanying EULA: https://corefonts.sourceforge.net/eula.htm (also `EULA.html`).
- SHA-256: `db56595ec6ef5d3de5c24994f001f03b2a13e37cee27bc25c58f6f43e8f807ab`.
- MD5: `ed39c8ef91b9fb80f76f702568291bd5`, matching the project's `msttcorefonts-2.5-1.spec`.

The complete, unchanged original installer and its original license are embedded
in the executable. Standalone TTFs are **not** redistributed in the application.
After explicit acceptance of the EULA, the Linux installer only extracts the four
original TTFs into the current user's font directory; it never runs the EXE.
The original names and contents of the fonts remain unchanged.

The EULA requires complete copies with copyright/trademark notices and the EULA.
It prohibits distribution for profit, including as part of another product.
This bundle is consequently intended only for distribution that complies with
those terms. Do not ship it with a paid product; configure
`-DSTELLARSUE_BUNDLE_TIMES=OFF` or obtain appropriate separate licensing instead.
These are the historical Core Fonts for the Web files, not fonts copied from a
current Windows installation. They do not include Chinese glyphs.

Runtime extraction requires `cabextract` or `bsdtar`; `fc-cache` refreshes discovery
for other desktop applications. Both font installation and cache updates run as
the current user, with no root access and no network download. Installation adds
the font to the user's available fonts without changing anyone's default fonts.
