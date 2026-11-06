#include "highlighter.h"

#include <QStringList>

static QTextCharFormat makeFormat(const QColor &color, bool bold = false, bool italic = false) {
    QTextCharFormat f;
    f.setForeground(color);
    if (bold)
        f.setFontWeight(QFont::DemiBold);
    f.setFontItalic(italic);
    return f;
}

static QString words(const QStringList &list) {
    return "\\b(?:" + list.join('|') + ")\\b";
}

Highlighter::Highlighter(QTextDocument *document, const QString &language, const QColor &accent)
    : QSyntaxHighlighter(document) {
    const QTextCharFormat keyword = makeFormat(accent, true);
    const QTextCharFormat string = makeFormat(QColor("#a6d189"));
    const QTextCharFormat number = makeFormat(QColor("#ef9f76"));
    const QTextCharFormat type = makeFormat(QColor("#8caaee"));
    const QTextCharFormat key = makeFormat(QColor("#99d1db"));
    m_comment = makeFormat(QColor("#7b7f8c"), false, true);

    auto add = [&](const QString &pattern, const QTextCharFormat &f, int group = 0,
                   QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption) {
        m_rules.append({QRegularExpression(pattern, options), f, group});
    };

    if (language == "c") {
        add(words({"if", "else", "for", "while", "do", "return", "switch", "case", "break", "continue", "default",
                   "class", "struct", "enum", "union", "namespace", "public", "private", "protected", "static",
                   "const", "constexpr", "virtual", "override", "final", "new", "delete", "this", "true", "false",
                   "null", "nullptr", "undefined", "let", "var", "function", "fn", "func", "import", "export",
                   "from", "package", "use", "mod", "impl", "trait", "pub", "async", "await", "yield", "try",
                   "catch", "throw", "throws", "finally", "typeof", "instanceof", "in", "of", "interface", "type",
                   "extends", "implements", "super", "match", "where", "loop", "mut", "ref", "self", "Self", "go",
                   "defer", "chan", "select", "range", "template", "typename", "using", "include", "define",
                   "signals", "slots", "emit", "property", "readonly", "required", "abstract", "sealed", "when",
                   "val", "inline", "extern", "goto", "sizeof", "volatile", "operator", "friend", "explicit"}),
            keyword);
        add(words({"void", "int", "char", "float", "double", "bool", "long", "short", "unsigned", "signed", "auto",
                   "string", "String", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64", "usize",
                   "isize", "size_t", "boolean", "number", "any", "object", "Q[A-Z]\\w*", "std"}),
            type);
        add("\\b(?:0x[0-9a-fA-F_]+|\\d[\\d_]*(?:\\.\\d+)?(?:[eE][+-]?\\d+)?)[a-zA-Z]*\\b", number);
        add("\"(?:[^\"\\\\]|\\\\.)*\"|'(?:[^'\\\\]|\\\\.)*'|`[^`]*`", string);
        add("//.*$", m_comment);
        m_blockStart = QRegularExpression("/\\*");
        m_blockEnd = QRegularExpression("\\*/");
    } else if (language == "hash") {
        add(words({"def", "class", "return", "if", "elif", "else", "for", "while", "in", "not", "and", "or", "is",
                   "import", "from", "as", "with", "try", "except", "finally", "raise", "lambda", "yield", "pass",
                   "break", "continue", "None", "True", "False", "self", "async", "await", "global", "nonlocal",
                   "then", "fi", "do", "done", "case", "esac", "function", "local", "export", "echo", "end",
                   "module", "require", "begin", "rescue", "unless", "until", "elsif", "true", "false", "null",
                   "set", "unset", "source", "exit", "shift", "readonly", "declare", "match"}),
            keyword);
        add("^\\s*([\\w.\\-]+)\\s*[=:](?!=)", key, 1);
        add("^\\s*\\[[^\\]]+\\]", type);
        add("\\$\\{?[A-Za-z_][A-Za-z0-9_]*\\}?", type);
        add("\\b\\d+(?:\\.\\d+)?\\b", number);
        add("\"(?:[^\"\\\\]|\\\\.)*\"|'[^']*'", string);
        add("(?:^|\\s)(#.*)$", m_comment, 1);
    } else if (language == "dash") {
        add(words({"select", "from", "where", "insert", "into", "values", "update", "set", "delete", "create",
                   "table", "drop", "alter", "join", "left", "right", "inner", "outer", "on", "group", "by",
                   "order", "having", "limit", "as", "and", "or", "not", "null", "primary", "key", "index",
                   "distinct", "union", "case", "when", "then", "else", "end", "local", "function", "return",
                   "if", "elseif", "for", "while", "do", "nil", "true", "false", "in", "repeat", "until",
                   "module", "where", "let", "data", "import", "type", "begin", "commit", "default", "references"}),
            keyword, 0, QRegularExpression::CaseInsensitiveOption);
        add("\\b\\d+(?:\\.\\d+)?\\b", number);
        add("\"(?:[^\"\\\\]|\\\\.)*\"|'(?:[^']|'')*'", string);
        add("--.*$", m_comment);
        m_blockStart = QRegularExpression("/\\*|--\\[\\[|\\{-");
        m_blockEnd = QRegularExpression("\\*/|\\]\\]|-\\}");
    } else if (language == "markup") {
        add("</?[A-Za-z_][\\w:.\\-]*|/?>", keyword);
        add("\\s([A-Za-z_][\\w:.\\-]*)(?==)", key, 1);
        add("\"[^\"]*\"|'[^']*'", string);
        add("&\\w+;|&#\\d+;", number);
        m_blockStart = QRegularExpression("<!--");
        m_blockEnd = QRegularExpression("-->");
    } else if (language == "json") {
        add("\"(?:[^\"\\\\]|\\\\.)*\"", string);
        add("(\"(?:[^\"\\\\]|\\\\.)*\")\\s*:", key, 1);
        add("\\b(?:true|false|null)\\b", keyword);
        add("-?\\b\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?\\b", number);
    }
}

void Highlighter::highlightBlock(const QString &text) {
    for (const auto &rule : m_rules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next();
            setFormat(int(match.capturedStart(rule.group)), int(match.capturedLength(rule.group)), rule.format);
        }
    }

    // Block comments span lines, so the state carries from one to the next.
    setCurrentBlockState(0);
    if (!m_blockStart.isValid() || m_blockStart.pattern().isEmpty())
        return;
    qsizetype start = 0;
    if (previousBlockState() != 1) {
        const auto match = m_blockStart.match(text);
        start = match.hasMatch() ? match.capturedStart() : -1;
    }
    while (start >= 0) {
        const auto end = m_blockEnd.match(text, start + (previousBlockState() == 1 && start == 0 ? 0 : 2));
        qsizetype length;
        if (!end.hasMatch()) {
            setCurrentBlockState(1);
            length = text.size() - start;
        } else {
            length = end.capturedEnd() - start;
        }
        setFormat(int(start), int(length), m_comment);
        const auto next = m_blockStart.match(text, start + length);
        start = next.hasMatch() ? next.capturedStart() : -1;
    }
}
