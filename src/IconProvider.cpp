#include "IconProvider.h"

#include <QFileIconProvider>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QByteArray>
#include <QBuffer>

namespace {
// 统一输出的图标画布边长, 与表格显示尺寸的高分屏放大倍数匹配
constexpr int kIconCanvasSize = 80;

// 非透明像素的包围盒。Windows 对没有大尺寸图标资源的 exe 会返回
// "小图标贴在大画布角落"的位图, 直接缩放会显得很小, 需先裁掉透明边
QRect opaqueBoundingRect(const QImage &image) {
    int minX = image.width(), minY = image.height();
    int maxX = -1, maxY = -1;

    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) > 8) {
                minX = qMin(minX, x);
                maxX = qMax(maxX, x);
                minY = qMin(minY, y);
                maxY = qMax(maxY, y);
            }
        }
    }

    if (maxX < 0) {
        return QRect();
    }
    return QRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
}
}

IconProvider::IconProvider(QObject *parent) : QObject(parent) {}

QString IconProvider::getExeIcon(const QString &filePath) {
    // 文件不存在(已卸载/移动盘未接入)时返回空,
    // 调用方保留原有图标, 避免被通用占位图标永久覆盖
    if (!QFileInfo::exists(filePath)) {
        return QString();
    }

    QFileIconProvider iconProvider;
    QIcon icon = iconProvider.icon(QFileInfo(filePath));

    // 不同 exe 内置的图标资源尺寸不一 (16~256 不等), 取可用的最大尺寸;
    // 枚举不到尺寸时至少按画布尺寸请求 (QIcon 只会向下取, 不会放大)
    QSize bestSize(kIconCanvasSize, kIconCanvasSize);
    const auto sizes = icon.availableSizes();
    for (const QSize &size: sizes) {
        if (size.width() > bestSize.width()) {
            bestSize = size;
        }
    }

    QPixmap pixmap = icon.pixmap(bestSize);
    if (pixmap.isNull()) {
        return QString();
    }

    QImage source = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    source.setDevicePixelRatio(1.0);

    const QRect content = opaqueBoundingRect(source);
    if (content.isEmpty()) {
        return QString();
    }

    // 裁掉透明边后统一等比缩放并居中到固定画布,
    // 保证表格中所有图标显示大小一致
    const QImage scaled = source.copy(content).scaled(
            kIconCanvasSize, kIconCanvasSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QImage canvas(kIconCanvasSize, kIconCanvasSize, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    painter.drawImage((kIconCanvasSize - scaled.width()) / 2,
                      (kIconCanvasSize - scaled.height()) / 2, scaled);
    painter.end();

    // 转换为 Base64
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    canvas.save(&buffer, "PNG");

    return "data:image/png;base64," + QString(byteArray.toBase64());
}
