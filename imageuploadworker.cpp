#include "imageuploadworker.h"
#include "global.h"

#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QUrl>

namespace {
struct EncodedImagePayload {
    QByteArray requestBody;
    QString errorMessage;
    bool success = false;
};

EncodedImagePayload encodeImageForUpload(const QImage &image, const QString &uploadId)
{
    EncodedImagePayload payload;
    if (image.isNull()) {
        payload.errorMessage = QStringLiteral("图片数据无效。");
        return payload;
    }

    constexpr int kMaxEncodedBytes = 30 * 1024 * 1024;
    const QImage normalized = image.convertToFormat(QImage::Format_RGBA8888);

    auto buildRequestBody = [&uploadId](const QString &contentEncoding, const QByteArray &content) {
        QJsonObject imageObj;
        imageObj["upload_id"] = uploadId;
        imageObj["content_encoding"] = contentEncoding;
        imageObj["content"] = QString::fromLatin1(content);
        return QJsonDocument(imageObj).toJson(QJsonDocument::Compact);
    };

    QByteArray pngBytes;
    {
        QBuffer pngBuffer(&pngBytes);
        pngBuffer.open(QIODevice::WriteOnly);
        if (normalized.save(&pngBuffer, "PNG")) {
            const QByteArray pngPayload = qCompress(pngBytes, 9).toBase64();
            if (pngPayload.size() <= kMaxEncodedBytes) {
                payload.requestBody = buildRequestBody(QStringLiteral("zlib+png"), pngPayload);
                payload.success = true;
                return payload;
            }
        }
    }

    QImage jpegSource = normalized;
    if (jpegSource.hasAlphaChannel()) {
        QImage flattened(jpegSource.size(), QImage::Format_RGB888);
        flattened.fill(Qt::white);
        QPainter painter(&flattened);
        painter.drawImage(QPoint(0, 0), jpegSource);
        painter.end();
        jpegSource = flattened;
    } else {
        jpegSource = jpegSource.convertToFormat(QImage::Format_RGB888);
    }

    const QList<int> jpegQualities = { 92, 85, 78, 70, 62, 54, 46, 38, 30 };
    for (int quality : jpegQualities) {
        QByteArray jpegBytes;
        QBuffer jpegBuffer(&jpegBytes);
        jpegBuffer.open(QIODevice::WriteOnly);
        if (!jpegSource.save(&jpegBuffer, "JPEG", quality)) {
            continue;
        }

        const QByteArray encoded = jpegBytes.toBase64();
        if (encoded.size() <= kMaxEncodedBytes) {
            payload.requestBody = buildRequestBody(QStringLiteral("jpeg"), encoded);
            payload.success = true;
            return payload;
        }
    }

    payload.errorMessage = QStringLiteral("图片过大，请尝试更小的图片。");
    return payload;
}
} // namespace

ImageUploadWorker::ImageUploadWorker(QObject *parent)
    : QObject(parent)
{
}

void ImageUploadWorker::uploadImage(const QString &gateUrlPrefix, const QString &uploadId, const QImage &image)
{
    if (!m_manager) {
        m_manager = new QNetworkAccessManager(this);
    }
    const EncodedImagePayload payload = encodeImageForUpload(image, uploadId);
    if (!payload.success || payload.requestBody.isEmpty()) {
        emit uploadFailed(uploadId,
                          payload.errorMessage.isEmpty()
                              ? QStringLiteral("图片过大，请尝试更小的图片。")
                              : payload.errorMessage);
        return;
    }

    QNetworkRequest request(QUrl(gateUrlPrefix + "/upload_image"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(payload.requestBody.size()));

    QNetworkReply *reply = m_manager->post(request, payload.requestBody);
    m_pendingReplies.insert(reply, uploadId);

    // 异步：reply 完成时通知我们，不阻塞任何线程
    connect(reply, &QNetworkReply::finished, this, &ImageUploadWorker::onReplyFinished);
}

void ImageUploadWorker::onReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }

    const QString uploadId = m_pendingReplies.take(reply);
    reply->deleteLater();

    if (uploadId.isEmpty()) {
        // 不在追踪列表里，忽略
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString message = reply->errorString();
        emit uploadFailed(uploadId,
                          message.isEmpty() ? QStringLiteral("图片上传失败，请稍后再试。") : message);
        return;
    }

    const QByteArray responseBytes = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(responseBytes);
    if (doc.isNull() || !doc.isObject()) {
        emit uploadFailed(uploadId, QStringLiteral("图片上传回包解析失败。"));
        return;
    }

    const QJsonObject obj = doc.object();
    if (obj.value("error").toInt() != ErrorCodes::SUCCESS) {
        emit uploadFailed(uploadId,
                          obj.value("message").toString(QStringLiteral("图片上传失败，请稍后再试。")));
        return;
    }

    const QString resourceKey = obj.value("resource_key").toString().isEmpty()
        ? obj.value("path").toString()
        : obj.value("resource_key").toString();
    if (resourceKey.isEmpty()) {
        emit uploadFailed(uploadId, QStringLiteral("图片上传结果无效。"));
        return;
    }

    emit uploadSucceeded(uploadId, resourceKey);
}
