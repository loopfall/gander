# Contributing

Bug reports with the file that misbehaves (or a description of it) are the
most useful thing. Use the issue templates.

For code:

1. `bin/build` builds `build/gander`; `bin/test` runs the tests offscreen.
2. Stop the installed service while you run your build, so yours owns the
   D-Bus name: `systemctl --user stop gander`, then
   `GANDER_TRACE=1 QT_FORCE_STDERR_LOGGING=1 build/gander daemon`. Afterwards,
   `systemctl --user start gander`.
3. `GANDER_TRACE=1` prints how long every step takes. A change that makes the
   first frame slower needs a very good reason.
4. `QT_QPA_PLATFORM=offscreen GANDER_SNAPSHOT=/tmp/shot.png build/gander daemon`
   saves a picture of every preview without opening a window. It's handy for
   checking layout changes.
5. New user-visible text goes through `qsTr()` in QML or
   `QCoreApplication::translate("gander", …)` in C++. Add translations to
   `src/i18n/*.json` if you speak the language.

Comments explain why, in full sentences. Commit messages are short and
imperative ("Preview HEIC photos").

To add a language, copy `src/i18n/sk.json` to the language's Omarchy code,
translate the values, and list the file in `src/resources.qrc`.
