#ifndef CHATINPUTEDIT_H
#define CHATINPUTEDIT_H

#include <QImage>
#include <QTextEdit>

class QMimeData;

class ChatInputEdit : public QTextEdit
{
    Q_OBJECT

public:
    explicit ChatInputEdit(QWidget *parent = nullptr);
    bool hasPendingImage() const;
    QImage takePastedImage();

signals:
    void imagePasted();

protected:
    void insertFromMimeData(const QMimeData *source) override;

private:
    QImage _pendingImage;
};

#endif // CHATINPUTEDIT_H
