#ifndef IMAGEUPLOADWORKER_H
#define IMAGEUPLOADWORKER_H

#include <QImage>
#include <QMap>
#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

class ImageUploadWorker : public QObject
{
    Q_OBJECT

public:
    explicit ImageUploadWorker(QObject *parent = nullptr);

public slots:
    void uploadImage(const QString &gateUrlPrefix, const QString &uploadId, const QImage &image);

signals:
    void uploadSucceeded(const QString &uploadId, const QString &resourceKey);
    void uploadFailed(const QString &uploadId, const QString &message);

private slots:
    void onReplyFinished();

private:
    QNetworkAccessManager *m_manager = nullptr;
    // key: QNetworkReply* 指针值（用地址做唯一键），value: uploadId
    QMap<QNetworkReply *, QString> m_pendingReplies;
};

#endif // IMAGEUPLOADWORKER_H
