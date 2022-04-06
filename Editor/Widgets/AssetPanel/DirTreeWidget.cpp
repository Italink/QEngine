#include "DirTreeWidget.h"
#include "QStyledItemDelegate"
#include "QPainter"
#include "Toolkit\QSvgIcon.h"

class DireTreeItemDelegate :public QStyledItemDelegate {
protected:
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
		if (!index.isValid())
			return;
		QString text = index.data(Qt::DisplayRole).toString();
		painter->save();
		QColor highlight = option.palette.brush(QPalette::Highlight).color();
		if (option.state & QStyle::State_HasFocus) {
			if (option.state & QStyle::State_Selected) {
				painter->fillRect(option.rect, highlight);
			}
			if (option.state & QStyle::State_MouseOver) {
				painter->fillRect(option.rect, highlight);
			}
		}

		QRect iconRect = option.rect;
		iconRect.setWidth(iconRect.height());
		iconRect.adjust(1, 1, -1, -1);
		if (option.state & QStyle::State_Open)
			mDirOpenIcon.getIcon().paint(painter, iconRect);
		else
			mDirCloseIcon.getIcon().paint(painter, iconRect);
		QRect textRect = option.rect;
		textRect.setLeft(textRect.left() + textRect.height() + 5);
		painter->setPen(option.palette.brush(QPalette::Text).color());
		painter->drawText(textRect, Qt::AlignLeft, text);
		painter->restore();
	}

	QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
		return QStyledItemDelegate::sizeHint(option, index);
	}
private:
	QSvgIcon mDirOpenIcon = QSvgIcon(":/Resources/Icons/24gf-folderOpen.png");
	QSvgIcon mDirCloseIcon = QSvgIcon(":/Resources/Icons/24gf-folder3.png");
};

DirTreeWidget::DirTreeWidget(QString rootDir)
	:rootDir_(rootDir) {
	setHeaderHidden(true);
	setFrameShape(QFrame::NoFrame);
	setIconSize(QSize(16, 16));
	setIndentation(10);
	intiDirectories();
	setItemDelegate(new DireTreeItemDelegate);
}

void DirTreeWidget::setCurrentDir(QString dir)
{
	auto ret = itemMap_.find(dir);
	if (ret != itemMap_.end()) {
		setCurrentItem(*ret);
	}
}

void DirTreeWidget::searchDir(QString keyword)
{
	if (keyword.isEmpty()) {
		for (auto& item : itemMap_) {
			item->setHidden(false);
		}
		return;
	}
	QList<QTreeWidgetItem*> itemList;
	for (auto& item : itemMap_) {
		item->setHidden(true);
		if (item->data(0, Qt::ToolTipRole).toString().contains(keyword)) {
			itemList << item;
		}
	}
	for (QTreeWidgetItem* curItem : itemList) {
		while (curItem != nullptr && curItem->isHidden()) {
			curItem->setHidden(false);
			curItem = curItem->parent();
			this->expandItem(curItem);
		}
	}
}

void DirTreeWidget::processDir(QDir dir, QTreeWidgetItem* item) {
	for (auto& subDir : dir.entryInfoList(QDir::Filter::Dirs | QDir::Filter::NoDotAndDotDot)) {
		if (subDir.isDir()) {
			QTreeWidgetItem* subItem = new QTreeWidgetItem({ subDir.fileName() });
			itemMap_[subDir.filePath()] = subItem;
			subItem->setData(0, Qt::ToolTipRole, subDir.filePath());
			item->addChild(subItem);
			processDir(subDir.filePath(), subItem);
		}
	}
}

void DirTreeWidget::intiDirectories() {
	for (auto& subDir : rootDir_.entryInfoList(QDir::Filter::Dirs | QDir::Filter::NoDotAndDotDot)) {
		if (subDir.isDir()) {
			QTreeWidgetItem* item = new QTreeWidgetItem({ subDir.fileName() });
			item->setData(0, Qt::ToolTipRole, subDir.filePath());
			itemMap_[subDir.filePath()] = item;
			addTopLevelItem(item);
			processDir(subDir.filePath(), item);
		}
	}
}