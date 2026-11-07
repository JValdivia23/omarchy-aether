#pragma once
#include "filepicker.h"
#include <QVariantMap>

class PortalFilePicker : public FilePicker {
    Q_OBJECT
public:
    explicit PortalFilePicker(QObject *parent = nullptr);
    ~PortalFilePicker() override;
    void openWallpaper() override;
    void openCollection() override;
private slots:
    void handleResponse(uint response, const QVariantMap &results);
private:
    void request(bool directory);
    void clear();
    bool subscribe(const QString &path);
    QString m_path;
    bool m_directory = false, m_pending = false;
    int m_generation = 0;
};
