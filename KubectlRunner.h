#include <QObject>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class KubectlRunner : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString podStatus READ podStatus NOTIFY dataChanged)

public:
    explicit KubectlRunner(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE void refresh() {
        QProcess *proc = new QProcess(this);
        // Ensure KUBECONFIG env var is set if not using default path
        proc->start("k3s", {"kubectl", "get", "pods", "-A", "-o", "json"});
        
        connect(proc, &QProcess::finished, [this, proc]() {
            QByteArray data = proc->readAllStandardOutput();
            parsePods(data);
            proc->deleteLater();
        });
    }

    QString podStatus() const { return m_podStatus; }

signals:
    void dataChanged();

private:
    void parsePods(const QByteArray &data) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray items = doc.object()["items"].toArray();
        // Rudimentary summary for the dashboard
        m_podStatus = QString("Pods Running: %1").arg(items.size());
        emit dataChanged();
    }

    QString m_podStatus;
};
