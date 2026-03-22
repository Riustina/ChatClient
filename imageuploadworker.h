#ifndef IMAGEUPLOADWORKER_H
#define IMAGEUPLOADWORKER_H

#include <QObject>
#include <QImage>

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
};

#endif // IMAGEUPLOADWORKER_H
