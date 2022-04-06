#ifndef QAeroWindowMaker_h__
#define QAeroWindowMaker_h__

#include <windows.h>        //ע��ͷ�ļ�
#include <QList>

class QWidget;

class QAeroWindowMaker {
public:
	static void make(QWidget *widget);
	static void setEnabled(bool var);
private:
	inline static QList<QWidget*> mWidgetList;
	inline static bool mEnabled = true;
};

#endif // QAeroWindowMaker_h__
