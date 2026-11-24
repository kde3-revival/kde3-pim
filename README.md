# kde3-pim

KMail, KOrganizer, KAddressBook and Kontact from KDE 3.5 (kdepim 3.5.10),
maintained by the [kde3-revival](https://github.com/kde3-revival) project
for today's Linux systems.

It is built on [kde3-libs](https://github.com/kde3-revival/kde3-libs) and
[kde3-qt](https://github.com/kde3-revival/kde3-qt) and needs
[kde3-base](https://github.com/kde3-revival/kde3-base) at run time (KMail
sends and fetches mail through its pop3 and smtp I/O slaves; KAddressBook's
LDAP search uses its ldap I/O slave).

The first commit of this repository is the pristine kdepim 3.5.10 release;
everything after it is this project's work, so `git log` and `git blame`
lead back to the original code. How the project works is described on the
[organisation page](https://github.com/kde3-revival).

## Contents

| Directory | What it is |
|---|---|
| `kmail` | The mail client |
| `korganizer` | Calendar, to-do list and journal, with the alarm daemon client (korgac) |
| `kaddressbook` | The address book application and part |
| `kontact` | The shell that joins KMail, KOrganizer and KAddressBook, with the Summary and Special Dates pages |
| `kmailcvt` | Import of mail from other programs (KMail's File > Import) |
| `certmanager` | Kleopatra (certificate manager), KWatchGnuPG and libkleopatra, the crypto backends of KMail |
| `kioslaves` | The imap4, mbox and sieve I/O slaves |
| `kresources` | Calendar and address book resources: remote iCalendar file, Kolab, birthdays |
| `libkcal` | iCalendar and vCalendar library, with a bundled libical |
| `libkmime`, `mimelib` | MIME message libraries |
| `libkdepim`, `libkpimidentities`, `libemailfunctions` | Code shared by the applications: widgets, identities, address helpers |
| `libkdenetwork` | gpgme++ and QGpgME, C++ bindings for GPGME |
| `libkpgp`, `libksieve`, `libkholidays`, `ktnef`, `kdgantt`, `indexlib` | OpenPGP helpers, Sieve parser, holidays, TNEF (winmail.dat), Gantt views, full-text index |
| `plugins` | KMail's body part formatters |
| `doc`, `pixmaps` | Handbooks, icons |

## Changes compared with KDE 3.5.10

### Build

- CMake replaces autotools; kdepim finds kdelibs through its CMake
  package (`find_package(KDE3Libs 3.5)`) and installs into the same
  prefix.
- Builds with current GCC and system libraries; uses the system's GPGME
  (also GPGME 2) instead of bundled copies of gpgme and libgpg-error.

### KMail: message reader

- Messages are shown with WPE WebKit by default, through a separate
  WebKit part for KDE 3, KWPEPart, from
  [kde3-webpart](https://github.com/kde3-revival/kde3-webpart). KHTML stays available ("Show messages with" in Security >
  Reading > HTML Messages, or `[Reader] HtmlEngine=khtml`) and is used
  when the part is not installed; printing still goes through KHTML.
- Each HTML part of a message and each HTML attachment viewed is a
  separate, sandboxed document without scripts, so a message's style
  sheet cannot reach KMail's headers and attachment list.
- multipart/related (RFC 2387): the images the HTML shows are displayed
  in place and no longer listed as attachments; `cid:` references are
  resolved as browsers decode them, within bounds on size and number.
- Text taken from a message (attachment names, signer ids, spam filter
  titles) is quoted before it goes into KMail's own document; KMail's
  internal links carry a per-document secret.
- The per-message link that loaded external references is gone; external
  references load only where the settings or the folder allow them.

### KMail: IMAP and folders

- Fewer round trips: kio_imap4 keeps a folder's type for the connection,
  sends no NOOP before a SELECT and no CLOSE before STATUS, RENAME or
  DELETE; KMail skips the envelope fetch of a folder with nothing new.
- With load on demand, only the images the shown HTML embeds are fetched,
  not the whole message.
- Large folders open faster (no linear searches per message, threads with
  unread messages opened while the list is filled).
- Dead connections are detected instead of hanging; a broken connection
  is reported once.
- Damaged or truncated folder indexes are detected and rebuilt; index
  writes are checked and made durable; serial numbers survive reconciles.
- KMail disconnects before the machine sleeps and checks mail after it
  wakes (systemd-logind).
- The tray icon brings the window to the current desktop at once.

### KOrganizer

- New calendars are downloaded from the KDE Store; downloaded calendars
  keep only display alarms (no programs, sounds or mail from strangers).

### Removed

To keep the package to KMail, KOrganizer, KAddressBook and Kontact:
Akregator, KAlarm, Kandy, KArm, KitchenSync, KMobile, KNode, KNotes,
KonsoleKalendar, KOrn, KPilot, the KTNEF viewer (the library stays),
Komposer, kode, libkpimexchange, kgantt, the networkstatus module, the
kabc tools, the file dialog plugins, the groupware resources other than
Kolab and remote files (eGroupware, Exchange, SLOX, GroupWise, Scalix,
GroupDAV), the groupware wizards, the Chiasmus backend, the KDE 2 address
book import, the Kontact plugins of the removed applications, and the
tests and examples of the 3.5.10 tarball.

## Requirements

- kde3-libs installed (which brings kde3-qt and the CMake macros), CMake
  3.20 or later, a C and C++ compiler, pkg-config, Perl (to generate
  libical's sources).
- Required: GPGME 0.4.5 or later and libgpg-error.
- Optional, used when found: cyrus-sasl 2 (IMAP and Sieve authentication),
  gio-unix-2.0 (KMail's sleep handling; needs kde3-qt running on the GLib
  event loop, its default), gnokii (mobile phones in KAddressBook), Boost
  headers (indexlib).
- At run time: kde3-base (pop3, smtp and ldap I/O slaves) and GnuPG (gpg,
  gpgsm, gpg-agent) for signing and encryption. The WebKit reader needs
  KWPEPart from kde3-webpart installed; without it KMail uses KHTML.

## Building and installing

Build and install kde3-qt, kde3-libs and kde3-base first, then:

    cmake -S . -B build
    cmake --build build -j"$(nproc)"
    sudo cmake --install build

kdepim installs into the prefix of the kdelibs it found, by default
`/opt/kde3`. If kdelibs is not in a standard place, pass
`-DCMAKE_PREFIX_PATH=/opt/kde3`.

Options:

| Option | Default | Effect |
|---|---|---|
| `KDEPIM_WITH_SASL` | empty | cyrus-sasl 2: empty uses it if found, `OFF` disables it, `ON` requires it |
| `KDEPIM_WITH_LOGIND` | empty | KMail's suspend/resume handling (gio-unix-2.0), same values |
| `KDEPIM_WITH_GNOKII` | empty | Mobile phones in KAddressBook (gnokii), same values |
| `KDEPIM_WITH_INOTIFY` | ON | inotify in KMail's external editor watcher |
| `KDEPIM_INDEXLIB` | OFF | Experimental full-text index in KMail |
| `KDEPIM_NEW_DISTRLISTS` | ON | Distribution lists saved as contacts in the address book |
| `KDEPIM_BROKEN_TIMSIEVED_WORKAROUND` | OFF | Make the sieve I/O slave work with timsieved 1.1.0 and older |

## Packages

[kde3-packaging](https://github.com/kde3-revival/kde3-packaging) builds
this repository for Fedora (`kde3-kdepim`, `kde3-kdepim-devel`) and Arch
Linux (`kde3-kdepim`). The packages depend on kde3-kdebase and GnuPG and
are built with cyrus-sasl and logind support, without gnokii and without
the full-text index.

## Status

KMail, KOrganizer and KAddressBook are used every day on the
kde3-revival desktop, on current Fedora and Arch Linux. Known
limitations:

- With the WebKit reader, printing uses KHTML.
- There is no CalDAV or CardDAV support yet; calendars on a server are
  reached as remote iCalendar files or through Kolab.
- `README.Kolab` and `README.libical` are the original notes of
  kdepim 3.5.10.

## License

The applications are under the GNU General Public License (GPL), the
libraries mostly under the GNU Library General Public License (LGPL);
see `korganizer/COPYING` (GPL) and `libkcal/COPYING` (LGPL), and the
header of each file. libical in `libkcal/libical` (see `README.libical`)
keeps the terms stated in its file headers.
Contributions are accepted under the same terms.

## Related repositories

- [kde3-qt](https://github.com/kde3-revival/kde3-qt): Qt 3.3.8b, the base
  of everything
- [kde3-libs](https://github.com/kde3-revival/kde3-libs): the KDE core
  libraries
- [kde3-base](https://github.com/kde3-revival/kde3-base): the desktop
  (KWin, Kicker, KDesktop, Konqueror, Konsole, KControl, ...)
- [kde3-apps](https://github.com/kde3-revival/kde3-apps): applications
- [kde3-webpart](https://github.com/kde3-revival/kde3-webpart): the WPE WebKit part
  (KWPEPart) used by KMail's reader and Konqueror
- [kde3-packaging](https://github.com/kde3-revival/kde3-packaging): Fedora
  and Arch Linux packages
