#ifndef KUBECTLRUNNER_H
#define KUBECTLRUNNER_H

#include <QObject>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <QTimer> // Added missing header
#include <QCoreApplication>  // <--- ADD THIS LINE
class KubectlRunner : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList podList READ podList NOTIFY podListChanged)
    Q_PROPERTY(bool k3sRunning READ k3sRunning NOTIFY k3sStatusChanged)

public:
    explicit KubectlRunner(QObject *parent = nullptr) : QObject(parent) {}

    bool k3sRunning() const { return m_k3sRunning; }
    QVariantList podList() const { return m_podList; }

    Q_INVOKABLE void startK3s() {
        // Run the start script detached so it persists after this app closes
        QProcess::startDetached("/home/root/start_k3s.sh");
        
        // Give k3s 5 seconds to initialize before the first refresh
        QTimer::singleShot(5000, this, &KubectlRunner::refresh);
    }

    Q_INVOKABLE void refresh() {
        QProcess *proc = new QProcess(this);
        
        // Setting KUBECONFIG here ensures kubectl knows where to look
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("KUBECONFIG", "/etc/rancher/k3s/k3s.yaml");
        proc->setProcessEnvironment(env);

        // Using 'k3s kubectl' is often more reliable on-device
        proc->start("k3s", {"kubectl", "get", "pods", "-A", "-o", "json"});
        
        connect(proc, &QProcess::finished, [this, proc]() {
            if (proc->exitCode() != 0) {
                m_k3sRunning = false;
                m_podList.clear();
            } else {
                m_k3sRunning = true;
                parsePods(proc->readAllStandardOutput());
            }
            emit k3sStatusChanged();
            emit podListChanged();
            proc->deleteLater();
        });
    }

    // Add the exit function we discussed for a clean return to xochitl
    Q_INVOKABLE void exitToSystem() {
        QProcess::execute("/home/root/stop_k3s.sh");
        QProcess::startDetached("systemctl", {"start", "xochitl"});
        QCoreApplication::quit();
    }

signals:
    void k3sStatusChanged();
    void podListChanged();

private:
    void parsePods(const QByteArray &data) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray items = doc.object()["items"].toArray();
        QVariantList newList;

        for (const QJsonValue &value : items) {
            QJsonObject obj = value.toObject();
            QVariantMap pod;
            pod["namespace"] = obj["metadata"].toObject()["namespace"].toString();
            pod["name"] = obj["metadata"].toObject()["name"].toString();
            pod["status"] = obj["status"].toObject()["phase"].toString();
            
            QJsonArray containerStatuses = obj["status"].toObject()["containerStatuses"].toArray();
            int readyCount = 0;
            for(const QJsonValue &cs : containerStatuses) {
                if(cs.toObject()["ready"].toBool()) readyCount++;
            }
            pod["ready"] = QString("%1/%2").arg(readyCount).arg(containerStatuses.size());
            
            newList.append(pod);
        }
        m_podList = newList;
    }

    bool m_k3sRunning = true;
    QVariantList m_podList;
};

#endif // KUBECTLRUNNER_H