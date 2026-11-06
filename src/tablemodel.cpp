#include "tablemodel.h"

void TableModel::setRows(const QList<QStringList> &rows) {
    beginResetModel();
    m_rows = rows;
    m_columns = 0;
    for (const auto &row : m_rows)
        m_columns = qMax(m_columns, int(row.size()));
    m_widths.fill(0, m_columns);
    // About 7.5 px a character at the table's font size, between a narrow
    // number column and a long description that would push the rest away.
    for (int r = 0; r < qMin<qsizetype>(m_rows.size(), 200); ++r)
        for (int c = 0; c < m_rows[r].size(); ++c)
            m_widths[c] = qMax(m_widths[c], int(m_rows[r][c].size()));
    for (auto &width : m_widths)
        width = qBound(56, int(width * 7.5) + 28, 340);
    endResetModel();
    emit changed();
}

int TableModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_rows.size());
}

int TableModel::columnCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_columns;
}

QVariant TableModel::data(const QModelIndex &index, int role) const {
    if (role != Qt::DisplayRole || !index.isValid() || index.row() >= m_rows.size())
        return {};
    return m_rows[index.row()].value(index.column());
}

QHash<int, QByteArray> TableModel::roleNames() const {
    return {{Qt::DisplayRole, "display"}};
}

int TableModel::columnWidth(int column) const {
    return m_widths.value(column, 120);
}
