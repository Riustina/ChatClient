#ifndef CHATINPUTEDIT_H
#define CHATINPUTEDIT_H

#include <QImage>
#include <QTextEdit>

class QMimeData;
class QKeyEvent;

class ChatInputEdit : public QTextEdit
{
    Q_OBJECT

public:
    explicit ChatInputEdit(QWidget *parent = nullptr);
    bool hasPendingImage() const;
    QImage takePastedImage();
    QString plainTextForSend() const;

signals:
    void imagePasted();
    void sendRequested();

protected:
    void insertFromMimeData(const QMimeData *source) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void insertImagePreview(const QImage &image);
    void syncPendingImageState();

    QImage _pendingImage;
};

#endif // CHATINPUTEDIT_H
