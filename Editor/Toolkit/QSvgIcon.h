#ifndef QSvgIcon_h__
#define QSvgIcon_h__

#include <functional>
#include <QList>
#include <QColor>
#include <QIcon>

class QSvgIcon {
public:
	QSvgIcon(QString path, QColor color = QColor(50,150,50));
	~QSvgIcon();

	using IconUpdateCallBack = std::function<void()>;

	const QIcon& getIcon();
	const QIcon& getIcon() const;

	void setUpdateCallBack(IconUpdateCallBack callback);

	void setPath(QString path);

	QColor getColor() const;
	void setColor(QColor val);
private:
	void updateIcon();
private:
	QString mPath;
	QColor mColor;
	QIcon mIcon;
	IconUpdateCallBack mCallBack;
	inline static QList<QSvgIcon*> mSvgIconList;
};  

#endif // QSvgIcon_h__
