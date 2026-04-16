#ifndef KUBECTLRUNNER_H
#define KUBECTLRUNNER_H

#include <QObject>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <QTimer>
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QSysInfo>
#include <QDebug>

class KubectlRunner : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList podList READ podList NOTIFY podListChanged)
    Q_PROPERTY(bool k3sRunning READ k3sRunning NOTIFY k3sStatusChanged)
    Q_PROPERTY(QString hostname READ hostname CONSTANT)

public:
    explicit KubectlRunner(QObject *parent = nullptr) : QObject(parent) {
        // Initial check to see if K3s is already alive
        bool currentlyRunning = checkK3sProcess();
        
        if (!currentlyRunning) {
            qDebug() << "k3s not detected on startup. Initializing...";
            startK3s();
        } else {
            qDebug() << "k3s already running. Fetching initial pod list...";
            m_k3sRunning = true;
            refresh();
        }
    }

    bool k3sRunning() const { return m_k3sRunning; }
    QVariantList podList() const { return m_podList; }
    QString hostname() const { return QSysInfo::machineHostName(); }

    Q_INVOKABLE void startK3s() {
        // Set state to true immediately so UI can show "Connecting..." or "Starting..."
        m_k3sRunning = true;
        emit k3sStatusChanged();
        // Use detached so the script lives on even if the dashboard blips
        QProcess::startDetached("/bin/bash", {"/home/root/start_k3s.sh"});
        // Give K3s time to initialize the API server before refreshing
        QTimer::singleShot(8000, this, &KubectlRunner::refresh);
    }

    Q_INVOKABLE void refresh() {
        // Start with the standard k3s location on the reMarkable
        runKubectl("/etc/rancher/k3s/k3s.yaml"); 
    }

    Q_INVOKABLE void exitToSystem() {
        qDebug() << "Exiting dashboard and restoring xochitl...";

        QProcess::execute("/home/root/stop_k3s.sh");
        // Start xochitl — the display service (Restart=on-failure) won't
        // restart on clean exit, and the monitor service (Restart=always)
        // will come back automatically and start watching for the next trigger.
        QProcess::startDetached("systemctl", {"start", "xochitl"});
        QCoreApplication::quit();
    }

signals:
    void k3sStatusChanged();
    void podListChanged();

private:
    // Helper to check for the k3s process name anywhere in the command line (-f)
    bool checkK3sProcess() {
        QProcess check;
        check.start("pgrep", {"-f", "k3s"});
        check.waitForFinished(1000); 
        return (check.exitCode() == 0);
    }
    void runKubectl(const QString &configPath) {
        QProcess *proc = new QProcess(this);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        
        // Ensure kubectl knows where to look
        env.insert("KUBECONFIG", configPath);
        // Force basic output to avoid terminal styling codes in the JSON
        env.insert("TERM", "dumb"); 
        proc->setProcessEnvironment(env);
    
        proc->start("/home/root/sbin/k3s", {"kubectl", "get", "pods", "-A", "-o", "json"});
        
        connect(proc, &QProcess::finished, [this, proc, configPath]() {
            bool success = (proc->exitStatus() == QProcess::NormalExit && proc->exitCode() == 0);
            
            if (success) {
                m_k3sRunning = true;
                parsePods(proc->readAllStandardOutput());
            } else {
                qWarning() << "Kubectl failed with error:" << proc->readAllStandardError();
                m_k3sRunning = false;
                m_podList.clear();
            }
            
            updateUI();
            proc->deleteLater();
        });
    }

    void updateUI() {
        emit k3sStatusChanged();
        emit podListChanged();
    }

    void parsePods(const QByteArray &data) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) return;

        QJsonArray items = doc.object()["items"].toArray();
        QVariantList newList;

        for (const QJsonValue &value : items) {
            QJsonObject obj = value.toObject();
            QVariantMap pod;
            
            // Extract Metadata
            QJsonObject metadata = obj["metadata"].toObject();
            pod["namespace"] = metadata["namespace"].toString();
            pod["name"] = metadata["name"].toString();
            
            // Extract Status
            QJsonObject statusObj = obj["status"].toObject();
            pod["status"] = statusObj["phase"].toString();
            
            // Calculate Ready Count
            QJsonArray containerStatuses = statusObj["containerStatuses"].toArray();
            int readyCount = 0;
            int totalCount = containerStatuses.size();
            
            for(const QJsonValue &cs : containerStatuses) {
                if(cs.toObject()["ready"].toBool()) {
                    readyCount++;
                }
            }
            
            // Format as "1/1" etc.
            pod["ready"] = QString("%1/%2").arg(readyCount).arg(totalCount > 0 ? totalCount : 0);
            
            newList.append(pod);
        }
        m_podList = newList;
    }

    bool m_k3sRunning = false;
    QVariantList m_podList;
};

#endif // KUBECTLRUNNER_H