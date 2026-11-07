#pragma once
#include <QObject>
#include <QUrl>

class FilePicker : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void openWallpaper() = 0;
    virtual void openCollection() = 0;
signals:
    void wallpaperSelected(const QUrl &url);
    void collectionSelected(const QUrl &url);
    void failed(const QString &message);
};
