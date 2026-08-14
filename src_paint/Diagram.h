#pragma once
#include "../src_baseClasses/dataStructUI.h"
#include "../src_baseClasses/DiagramData.h"
#include "../src_config/configUI.h"
#include <QWidget>
#include <QTimer>

class Diagram : public QWidget {
	Q_OBJECT
public:
	explicit Diagram(std::shared_ptr<DiagramData> data, ConfigUI* config, QWidget* parent = nullptr)
		: QWidget(parent), m_config(config), m_data(data) {
		m_scaleFactor = 1.0;
		setMouseTracking(true);
		setAttribute(Qt::WA_TransparentForMouseEvents, false);
		setContentsMargins(5, 5, 5, 5);
		setAutoFillBackground(true);
		setMinMax();
		setData1();
		setData2();
		// 延迟初始化视图变换
		QTimer::singleShot(0, this, &Diagram::initViewTransform);
	};
	~Diagram() {};

	void initViewTransform();


	// 数据更新
	void setMinMax();
	void setData1();
	void setData2();
	void setData3();
	void setShowTreeIndex(int index) { m_treeIndex = index; };
	void refresh();

	//public:
private:
	// 核心数据
	std::shared_ptr<DiagramData> m_data;
	ConfigUI* m_config;
	QPointF* m_minPt;
	QPointF* m_maxPt;

	//1.背景层绘图信息(第一优先级绘制的内容)
	std::vector<std::vector<QPointF>*> m_pts1;
	std::vector<PointStyle*> m_ss_pts1;
	std::vector<std::vector<LineUI>*> m_lines1;
	std::vector<LineStyle*> m_ss_lines1;
	std::vector<std::vector<LineUI>*> m_polys1;
	std::vector<PolygonStyle*> m_ss_polys1;
	std::vector<std::vector<LineUI>*> m_linePts1;
	std::vector<PointStyle*> m_ss_linePts1;
	std::vector<std::vector<CircleUI>*> m_circle1;
	std::vector<PolygonStyle*> m_ss_circle1;

	//2.前景层绘图信息(后绘制的内容，会将先绘制的内容挡住)
	std::vector<std::vector<QPointF>*> m_pts2;
	std::vector<PointStyle*> m_ss_pts2;
	std::vector<std::vector<LineUI>*> m_lines2;
	std::vector<LineStyle*> m_ss_lines2;
	std::vector<std::vector<LineUI>*> m_polys2;
	std::vector<PolygonStyle*> m_ss_polys2;
	std::vector<std::vector<LineUI>*> m_linePts2;
	std::vector<PointStyle*> m_ss_linePts2;
	std::vector<std::vector<CircleUI>*> m_circle2;
	std::vector<PolygonStyle*> m_ss_circle2;

	//3.动态显示数据（通过下拉框选择是否显示）
	int m_treeIndex = -1;
	std::vector<std::vector<LineUI>*> m_lines3;
	std::vector<LineStyle*> m_ss_lines3;




private:	//########################## 绘图模块 ##########################

	//0.1 视图变换数据
	QRectF m_visibleRect;		//可见区域（实际数据）
	QTransform m_transform;		//变换矩阵，数据的实际坐标-->屏幕坐标
	float m_scaleFactor = 1.0;	//缩放倍数，数据的实际坐标-->屏幕坐标
	QPointF m_panOffset;		//缩放后的偏移量（屏幕坐标偏移量）
	bool m_initialized = false;	//已完成视图初始化

	// 0.2  鼠标交互数据
	bool m_targetSelected = false;
	QPoint m_pressPos;		//按下瞬间捕捉的点
	QPoint m_releasePos;
	QPoint m_movePos;		//移动过程中捕捉的点
	QBasicTimer m_moveTimer;

	//0.2 绘图缓存
	QPixmap m_canvasCache;      // 离屏缓存图像
	QPoint m_cacheOffset;       // 缓存相对于当前视图的偏移
	bool m_cacheValid = false;  // 缓存是否有效

	//0.3绘图样式，//按层设置颜色
	std::vector<QColor> m_colors = { Qt::darkGray, Qt::red,  Qt::blue, Qt::darkYellow,
		Qt::green, Qt::magenta, Qt::darkRed,  Qt::black, Qt::cyan };
	QColor* getColor(int i) { return &m_colors[i % m_colors.size()]; }

	//1.可见区绘制，缓存绘制
	void paintVisibleContent(const QRect& rect, QPainter& painter);
	void paintContent(QPainter& painter);
	//2.绘制各个模块
	void drawGrids(QPainter& painter);	//绘制背景网格
	void drawSegments1(QPainter& painter);
	void drawSegments2(QPainter& painter);
	void drawSegments3(QPainter& painter);
	//3.绘制点,线，多边形
	void drawLines(std::vector<LineUI>* pts, LineStyle* s, QPainter& painter);
	void drawPoints(std::vector<QPointF>* pts, PointStyle* s, QPainter& painter);
	void drawPolygons(std::vector<LineUI>* lines, PolygonStyle* s, QPainter& painter);
	void drawLinePoints(std::vector<LineUI>* lines, PointStyle* s, QPainter& painter);
	void drawCircles(std::vector<CircleUI>* circles, PolygonStyle* s, QPainter& painter);

	//4.坐标转换接口
	QPointF dataToScreen(const QPointF& dataPoint) const;
	LineUI  dataToScreen(const LineUI& dataLine) const;
	QPointF screenToData(const QPointF& screenPoint) const;
	void updateTransform();

	//5.线段裁剪
	bool clipLine(QPointF& p1, QPointF& p2, const QRectF& viewport) const;
	bool clipLine(LineUI& line, const QRectF& viewport) const;
	bool clipPt(const QPointF& pt, const QRectF& viewport) const;

protected:	//########################## 事件管理 ##########################

	//1.综合事件
	void showEvent(QShowEvent* event) override;			//初始化显示
	void resizeEvent(QResizeEvent* event) override;		//resize
	void timerEvent(QTimerEvent* event) override;		//定时刷新
	void paintEvent(QPaintEvent* event) override;		//绘图事件

	//2.鼠标事件
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void leaveEvent(QEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;

	//3.事件执行
	void wheelEventRun(const int& delta, const QPointF& screenPos);

private:	//########################## 事件执行 ##########################

	//0.1推线参数
	SelectedTarget m_slLine;
	double m_snapThresholdPixels = 10.0;
	//鼠标左键，捕捉点或线，按下捕捉，释放返回偏移量和捕捉到的信息
	void checkHoverTarget(const QPointF& hoverPt);
	//鼠标左键，捕捉点或线，按下捕捉，释放返回偏移量和捕捉到的信息
	bool selectTargetBegin(const QPointF& clickPt);	//捕捉点或线
	bool selectTargetActivate();
	bool selectTargetEnd(const QPointF& releasePt);
	//鼠标中键，移动画布
	void moveCanvasBegin(const QPointF& clickPt);	//按下，执行一次
	void moveCanvasActivate();						//移动，持续执行
	void moveCanvasEnd(const QPoint& releasePt);	//释放，执行一次
	//鼠标右键，暂无

	//吸附阈值(数据坐标
	template<typename DataType, typename StyleType>
	bool selectTarget(const QPointF& hoverPt, const bool& hover, std::vector<DataType*>& dataVector, std::vector<StyleType*>& styleVector);

	bool setSelections(const QPointF& clickPt, const bool& hover, std::vector<QPointF>* candidates, PointStyle* s);
	bool setSelections(const QPointF& clickPt, const bool& hover, std::vector<LineUI>* candidates, LineStyle* s);
	bool setSelections(const QPointF& clickPt, const bool& hover, std::vector<LineUI>* candidates, PolygonStyle* s);
	bool setSelections(const QPointF& clickPt, const bool& hover, std::vector<LineUI>* candidates, PointStyle* s);

	bool setSelectedPt(const QPointF& clickPt, const std::vector<QPointF>& line, double snapThreshold);
	bool setSelectedPt(const QPointF& clickPt, const LineUI& line, double snapThreshold);
	bool setSelectedPt(const QPointF& clickPt, std::vector<LineUI>& lines, double snapThreshold);
	bool setSelectedPt(const QPointF& clickPt, std::vector<std::vector<LineUI>>& lines, double snapThreshold);
	bool setSelectedPt(const QPointF& clickPt, std::vector<std::vector<std::vector<LineUI>>>& lines, double snapThreshold);

	bool setSelectedLine(const QPointF& clickPt, const LineUI& line, double snapThreshold);
	bool setSelectedLine(const QPointF& clickPt, std::vector<LineUI>& lines, double snapThreshold);
	bool setSelectedLine(const QPointF& clickPt, std::vector<std::vector<LineUI>>& lines, double snapThreshold);
	bool setSelectedLine(const QPointF& clickPt, std::vector<std::vector<std::vector<LineUI>>>& lines, double snapThreshold);

signals:
	void viewChanged();
	void dataModified();
	void leftPressed(SelectedTarget* target);
	void leftPressedMove(SelectedTarget* target);
	void leftReleaseed(SelectedTarget* target);
};
