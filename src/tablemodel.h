#pragma once

#include <QAbstractTableModel>
#include <QList>
#include <QStringList>

// Rows of a CSV or TSV file for QML's TableView.
class TableModel : public QAbstractTableModel {
    Q_OBJECT
    Q_PROPERTY(int rows READ rowCountProperty NOTIFY changed)
    Q_PROPERTY(int columns READ columnCountProperty NOTIFY changed)

public:
    using QAbstractTableModel::QAbstractTableModel;

    void setRows(const QList<QStringList> &rows);
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // A width in pixels that fits the column's longest early value.
    Q_INVOKABLE int columnWidth(int column) const;

    int rowCountProperty() const { return rowCount(); }
    int columnCountProperty() const { return columnCount(); }

signals:
    void changed();

private:
    QList<QStringList> m_rows;
    QList<int> m_widths;
    int m_columns = 0;
};
