#pragma once

#include <QColor>
#include <QList>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

// A small, fast highlighter for the language families TextLoader names.
// It knows keywords, strings, numbers, comments and markup tags, which is what
// a preview needs; it isn't trying to be an editor's highlighter.
class Highlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    Highlighter(QTextDocument *document, const QString &language, const QColor &accent);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
        int group = 0;
    };
    QList<Rule> m_rules;
    QRegularExpression m_blockStart, m_blockEnd;
    QTextCharFormat m_comment;
};
