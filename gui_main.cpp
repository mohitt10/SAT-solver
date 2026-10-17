#include "parse_error.h"
#include "parser.h"

#include <QApplication>
#include <QColor>
#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QStringList>
#include <QTextCharFormat>
#include <QTextEdit>
#include <QTextCursor>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace {

QString categoryName(ErrorCategory category) {
    return category == ErrorCategory::LEXICAL ? "Lexical error" : "Syntax error";
}

QString expectedText(const std::vector<std::string>& expected) {
    QStringList values;
    for (const std::string& item : expected)
        values.push_back(QString::fromStdString(item));
    return values.join(", ");
}

int utf16PositionForByteOffset(const QString& text, std::size_t byteOffset) {
    const QByteArray utf8 = text.toUtf8();
    const auto boundedOffset = static_cast<int>(
        std::min(byteOffset, static_cast<std::size_t>(utf8.size())));
    return QString::fromUtf8(utf8.constData(), boundedOffset).size();
}

void showErrorLocation(QPlainTextEdit* editor, const ParseError& error) {
    const QString text = editor->toPlainText();
    const int start = utf16PositionForByteOffset(text, error.offset);
    const int end = error.found.empty()
                        ? start
                        : utf16PositionForByteOffset(
                              text, error.offset + error.found.size());

    QTextCursor cursor(editor->document());
    cursor.setPosition(start);
    if (end > start)
        cursor.setPosition(end, QTextCursor::KeepAnchor);
    QTextEdit::ExtraSelection selection;
    selection.cursor = cursor;
    selection.format.setBackground(QColor("#ffe2df"));
    selection.format.setUnderlineColor(QColor("#b42318"));
    selection.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    editor->setExtraSelections({selection});
}

} // namespace

int main(int argc, char* argv[]) {
    qunsetenv("GTK_PATH");
    qunsetenv("GIO_MODULE_DIR");

    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("SAT Solver — Formula Validator");
    window.resize(680, 440);

    auto* title = new QLabel("Propositional Formula Validator");
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto* instructions = new QLabel(
        "Enter a formula using variables, parentheses, and ~, &, |, ->, or <->.");
    instructions->setWordWrap(true);

    auto* editor = new QPlainTextEdit;
    editor->setPlaceholderText("For example: (A & ~B) -> C");
    editor->setTabChangesFocus(true);
    editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    auto* status = new QLabel;
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    status->setWordWrap(true);

    auto* layout = new QVBoxLayout(&window);
    layout->addWidget(title);
    layout->addWidget(instructions);
    layout->addWidget(editor, 1);
    layout->addWidget(status);

    QObject::connect(editor, &QPlainTextEdit::textChanged, &window,
                     [editor, status]() {
        const std::string formula = editor->toPlainText().toStdString();
        try {
            (void)parseFormula(formula);
            editor->setExtraSelections({});
            status->setStyleSheet("QLabel { color: #207245; }");
            status->setText("Valid formula");
        } catch (const ParseError& error) {
            QString details = QString("%1 at line %2, column %3: %4")
                                  .arg(categoryName(error.category))
                                  .arg(error.line)
                                  .arg(error.column)
                                  .arg(QString::fromStdString(error.message));
            if (!error.expected.empty())
                details += "\nExpected: " + expectedText(error.expected);
            status->setStyleSheet("QLabel { color: #b42318; }");
            status->setText(details);
            showErrorLocation(editor, error);
        } catch (const std::exception& error) {
            status->setStyleSheet("QLabel { color: #b42318; }");
            status->setText(QString("Unable to validate formula: %1")
                                .arg(QString::fromUtf8(error.what())));
        }
    });

    window.show();
    return app.exec();
}
