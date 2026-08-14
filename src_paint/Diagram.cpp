#include "Diagram.h"
#include "../src_config/DiagramStyle.h"
#include <QTimer>
#include <QTabWidget>
#include <cmath>
#include <memory>
#include <QMessageBox>
#include <QPainterPath>

using namespace std;

//视图初始化
void Diagram::initViewTransform() {
	QSize effectiveSize;
	if (QTabWidget* tabWidget = qobject_cast<QTabWidget*>(parentWidget())) {	// 通过父容器获取
		effectiveSize = tabWidget->currentWidget()->size();
	}
	else {
		effectiveSize = size();		//直接获取可用尺寸
	}
	if (effectiveSize.width() <= 10 || effectiveSize.height() <= 10) {
		effectiveSize = QSize(800, 600); // 默认尺寸
	}
	// 计算数据边界
	QRectF dataBounds(*m_minPt, *m_maxPt);

	// 计算初始缩放比例（保留5%边距）
	if (m_config->m_rotate_90) {
		qreal xScale = (effectiveSize.width() * 0.95) / dataBounds.height();  // 用高度计算x缩放
		qreal yScale = (effectiveSize.height() * 0.95) / dataBounds.width();  // 用宽度计算y缩放
		m_scaleFactor = qMin(xScale, yScale);
	}
	else {
		qreal xScale = (effectiveSize.width() * 0.95) / dataBounds.width();
		qreal yScale = (effectiveSize.height() * 0.95) / dataBounds.height();
		m_scaleFactor = qMin(xScale, yScale);
	}
	// 计算初始平移（居中）,即数据被缩放到相应大小，并且移动到最左侧之后，然后进行m_panOffset平移即可居中
	qreal offsetX = 0, offsetY = 0;
	qreal dataWidth = dataBounds.width() * m_scaleFactor;
	qreal dataHeight = dataBounds.height() * m_scaleFactor;
	qreal viewWidth = effectiveSize.width();
	qreal viewHeight = effectiveSize.height();
	if (m_config->m_flip_left_right) {
		if (m_config->m_flip_up_down) {
			if (m_config->m_rotate_90) {
				offsetX = (viewWidth - dataHeight) / 2 - m_minPt->y() * m_scaleFactor;
				offsetY = (viewHeight - dataWidth) / 2 + m_maxPt->x() * m_scaleFactor;
			}
			else {
				offsetX = (viewWidth + dataWidth) / 2 + m_minPt->x() * m_scaleFactor;
				offsetY = (viewHeight + dataHeight) / 2 + m_minPt->y() * m_scaleFactor;
			}
		}
		else {
			if (m_config->m_rotate_90) {
				offsetX = (viewWidth + dataHeight) / 2 + m_minPt->y() * m_scaleFactor;
				offsetY = (viewHeight - dataWidth) / 2 + m_maxPt->x() * m_scaleFactor;
			}
			else {
				offsetX = (viewWidth + dataWidth) / 2 + m_minPt->x() * m_scaleFactor;
				offsetY = (viewHeight - dataHeight) / 2 - m_minPt->y() * m_scaleFactor;
			}
		}
	}
	else {
		if (m_config->m_flip_up_down) {
			if (m_config->m_rotate_90) {
				offsetX = (viewWidth - dataHeight) / 2 - m_minPt->y() * m_scaleFactor;
				offsetY = (viewHeight + dataWidth) / 2 - m_maxPt->x() * m_scaleFactor;
			}
			else {
				offsetX = (viewWidth - dataWidth) / 2 - m_minPt->x() * m_scaleFactor;
				offsetY = (viewHeight + dataHeight) / 2 + m_minPt->y() * m_scaleFactor;
			}
		}
		else {
			if (m_config->m_rotate_90) {
				offsetX = (viewWidth + dataHeight) / 2 + m_minPt->y() * m_scaleFactor;
				offsetY = (viewHeight - dataWidth) / 2 - m_minPt->x() * m_scaleFactor;
			}
			else {
				offsetX = (viewWidth - dataWidth) / 2 - m_minPt->x() * m_scaleFactor;
				offsetY = (viewHeight - dataHeight) / 2 - m_minPt->y() * m_scaleFactor;
			}
		}
	}
	m_panOffset = QPointF(offsetX, offsetY);
	updateTransform();
}
void Diagram::setMinMax() {
	m_minPt = m_data->getMinPoint();
	m_maxPt = m_data->getMaxPoint();
}
void Diagram::setData1() {
	m_pts1.clear();
	m_ss_pts1.clear();
	m_lines1.clear();
	m_ss_lines1.clear();
	m_polys1.clear();
	m_ss_polys1.clear();
	m_linePts1.clear();
	m_ss_linePts1.clear();
	m_data->set_canvas_data1(m_pts1, m_ss_pts1, m_lines1, m_ss_lines1, m_polys1, m_ss_polys1, m_linePts1, m_ss_linePts1, m_circle1, m_ss_circle1);
	update();
}
void Diagram::setData2() {
	m_pts2.clear();
	m_ss_pts2.clear();
	m_lines2.clear();
	m_ss_lines2.clear();
	m_polys2.clear();
	m_ss_polys2.clear();
	m_linePts2.clear();
	m_ss_linePts2.clear();
	m_data->set_canvas_data2(m_pts2, m_ss_pts2, m_lines2, m_ss_lines2, m_polys2, m_ss_polys2, m_linePts2, m_ss_linePts2, m_circle2, m_ss_circle2);
	update();
}
void Diagram::setData3() {
	m_lines3.clear();
	m_ss_lines3.clear();
	m_data->set_canvas_data3(m_lines3, m_ss_lines3);
}
void Diagram::refresh() {
	update();
}

//########################## 绘图模块（私有） ##########################

//1.绘制模式
void Diagram::paintVisibleContent(const QRect& rect, QPainter& painter) {
	painter.save();
	//1.初始化
	m_visibleRect = m_transform.inverted().mapRect(rect);		//获取可视区域（数据坐标系）
	//2.绘制网格
	drawGrids(painter);
	//3.先后绘制其他图元
	drawSegments1(painter);
	drawSegments2(painter);
	drawSegments3(painter);
	painter.restore();
}
void Diagram::paintContent(QPainter& painter) {
	if (m_config->painState == PUSH_LINE)
		return;
	painter.setRenderHint(QPainter::Antialiasing);
	painter.save();
	QFont font = painter.font();
	font.setPointSize(9);
	painter.setFont(font);

	// 转换为屏幕坐标
	QPointF screenPos = dataToScreen(m_slLine.infoPosition);

	// 计算文本大小
	QFontMetrics fm(font);
	QRect textRect = fm.boundingRect(QRect(0, 0, 200, 100),
		Qt::AlignLeft | Qt::TextWordWrap,
		m_slLine.infoText);
	textRect.moveTo(screenPos.x() + 25, screenPos.y() + 10);

	// 绘制背景
	QRect bgRect = textRect.adjusted(-5, -5, 5, 5);
	painter.setBrush(QColor(255, 255, 225, 230));
	painter.setPen(QPen(Qt::black, 1));
	painter.drawRect(bgRect);

	// 绘制文本
	painter.setPen(Qt::black);
	painter.drawText(textRect, Qt::AlignLeft | Qt::TextWordWrap, m_slLine.infoText);
	painter.restore();
}

//2.绘制各个模块
void Diagram::drawGrids(QPainter& painter) {
	if (m_config->m_gridType == 0) return;	// 不显示网格
	if (!m_minPt || !m_maxPt) return;		// 检查必要的变量是否存在
	painter.save();
	painter.setRenderHint(QPainter::Antialiasing, false);
	float gridSize = m_config->m_gridSize;
	float screenGridSpacing = gridSize * m_scaleFactor;
	int step = 1;
	if (screenGridSpacing < 20.0f) {
		// 如果屏幕间距小于5像素，计算合适的步长
		step = static_cast<int>(std::ceil(m_config->m_minimalScreenGridSize / screenGridSpacing));
	}

	if (m_config->m_gridType == 1) {  //1.网格线
		painter.setPen(QPen(Qt::gray, 1)); // 网格线为灰色，1像素宽
		float startX = m_minPt->x();
		float startY = m_minPt->y();
		float endX = m_maxPt->x();
		float endY = m_maxPt->y();
		int xCount = static_cast<int>((endX - startX) / gridSize);
		int yCount = static_cast<int>((endY - startY) / gridSize);
		for (int i = 0; i <= xCount; i += step) {
			float x = startX + i * gridSize;
			painter.drawLine(dataToScreen(QPointF(x, startY)), dataToScreen(QPointF(x, endY)));
		}
		for (int i = 0; i <= yCount; i += step) {
			float y = startY + i * gridSize;
			painter.drawLine(dataToScreen(QPointF(startX, y)), dataToScreen(QPointF(endX, y)));
		}
	}
	else if (m_config->m_gridType == 2) {	 //2.网格点
		/**/
		painter.setPen(QPen(Qt::black, 1));
		// 计算网格点的起始位置（对齐到网格）
		float startX = std::floor(m_visibleRect.left() / gridSize) * gridSize;
		float startY = std::floor(m_visibleRect.top() / gridSize) * gridSize;
		float endX = m_visibleRect.right();
		float endY = m_visibleRect.bottom();
		int xCount = static_cast<int>((endX - startX) / gridSize) + 1;
		int yCount = static_cast<int>((endY - startY) / gridSize) + 1;
		painter.setBrush(QBrush(Qt::black));
		QPointF screenPoint;
		for (int i = 0; i < xCount; i += step) {
			float x = startX + i * gridSize;
			for (int j = 0; j < yCount; j += step) {
				float y = startY + j * gridSize;
				screenPoint = dataToScreen(QPointF(x, y));
				// 绘制4个点，加大点
				painter.drawRect(QRectF(screenPoint.x(), screenPoint.y(), 2, 2));
			}
		}
		painter.setBrush(Qt::NoBrush);
	}
	// 绘制边框
	painter.setPen(QPen(Qt::black, 3));
	QRectF borderRect(dataToScreen(*m_minPt), dataToScreen(*m_maxPt));
	painter.drawRect(borderRect);
	painter.restore();
}
void Diagram::drawSegments1(QPainter& painter) {
	// 1.绘制线
	for (int i = 0; i < m_lines1.size(); i++)
		if (m_ss_lines1[i]->isVisible())
			drawLines(m_lines1[i], m_ss_lines1[i], painter);
	// 2.绘制点
	for (int i = 0; i < m_pts1.size(); i++)
		if (m_ss_pts1[i]->isVisible())
			drawPoints(m_pts1[i], m_ss_pts1[i], painter);
	// 3.绘制多边形
	for (int i = 0; i < m_polys1.size(); i++)
		if (m_ss_polys1[i]->isVisible())
			drawPolygons(m_polys1[i], m_ss_polys1[i], painter);
	// 4.绘制线的端点
	for (int i = 0; i < m_linePts1.size(); i++)
		if (m_ss_linePts1[i]->isVisible())
			drawLinePoints(m_linePts1[i], m_ss_linePts1[i], painter);
	for (int i = 0; i < m_circle1.size(); i++)
		if (m_ss_circle1[i]->isVisible())
			drawCircles(m_circle1[i], m_ss_circle1[i], painter);
};
void Diagram::drawSegments2(QPainter& painter) {
	// 1.绘制线
	for (int i = 0; i < m_lines2.size(); i++)
		if (m_ss_lines2[i]->isVisible())
			drawLines(m_lines2[i], m_ss_lines2[i], painter);
	// 2.绘制点
	for (int i = 0; i < m_pts2.size(); i++)
		if (m_ss_pts2[i]->isVisible())
			drawPoints(m_pts2[i], m_ss_pts2[i], painter);
	// 3.绘制多边形
	for (int i = 0; i < m_polys2.size(); i++)
		if (m_ss_polys2[i]->isVisible())
			drawPolygons(m_polys2[i], m_ss_polys2[i], painter);
	// 4.绘制线的端点
	for (int i = 0; i < m_linePts2.size(); i++)
		if (m_ss_linePts2[i]->isVisible())
			drawLinePoints(m_linePts2[i], m_ss_linePts2[i], painter);
	for (int i = 0; i < m_circle2.size(); i++)
		if (m_ss_circle2[i]->isVisible())
			drawCircles(m_circle2[i], m_ss_circle2[i], painter);
}
void Diagram::drawSegments3(QPainter& painter) {
	// 绘制线
	if (!m_config->m_showTrees || m_lines3.empty())		// 不显示，或为空
		return;
	if (m_treeIndex == -1) {	// 全显示
		for (int i = 0; i < m_lines3.size(); i++)
			if (m_ss_lines3[i]->isVisible())
				drawLines(m_lines3[i], m_ss_lines3[i], painter);
		return;
	}
	if (m_treeIndex >= 0 && m_treeIndex < m_lines3.size()) {
		// 绘制指定索引的线
		drawLines(m_lines3[m_treeIndex], m_ss_lines3[m_treeIndex], painter);
		return;
	}
}

//3.绘制元素
void Diagram::drawLines(std::vector<LineUI>* lines, LineStyle* s, QPainter& painter) {
	// 设置线条属性
	QPen pen(s->m_color);
	pen.setWidthF(s->m_width);
	pen.setStyle(s->m_style);
	pen.setCapStyle(Qt::RoundCap);      // 消除尖角
	pen.setJoinStyle(Qt::RoundJoin);    // 消除尖角
	QPen penLine = pen;
	// 遍历所有线段并绘制
	for (LineUI line : *lines) {
		if (clipLine(line, m_visibleRect)) {
			QLineF screenLine(dataToScreen(line.p1()), dataToScreen(line.p2()));
			float width = line.width * m_scaleFactor;
			if (width > 1 && width < 400) {
				penLine.setWidthF(width);
				if (line.layer > 0) {
					penLine.setColor(*getColor(line.layer));
				}
				painter.setPen(penLine);
			}
			else {
				pen.setWidthF(s->m_width);  // 使用基础线宽
				if (line.layer > 0) {
					pen.setColor(*getColor(line.layer));
				}
				painter.setPen(pen);
			}
			painter.drawLine(screenLine);
		}
	}
}
void Diagram::drawPoints(std::vector<QPointF>* pts, PointStyle* s, QPainter& painter) {
	painter.save();
	// 设置点属性
	if (s->m_fill) 			// 设置填充
		painter.setBrush(QBrush(s->m_fillColor));
	else
		painter.setBrush(Qt::NoBrush);

	if (s->m_outlineShow) {	// 设置外框
		QPen pen(s->m_outlineColor);
		pen.setWidthF(s->m_outlineWidth);
		painter.setPen(pen);
	}
	else
		painter.setPen(Qt::NoPen);
	if (pts->size() < 400) {	// 数量少时，逐个绘制
		for (const QPointF& pt : *pts) {
			if (clipPt(pt, m_visibleRect)) {	// 1. 裁剪数据（点，直接过滤）
				painter.drawEllipse(dataToScreen(pt), s->m_r, s->m_r);// 绘制椭圆
			}
		}
	}
	else {		//数量多时，简化绘制流程
		painter.setRenderHint(QPainter::Antialiasing, false);
		QPointF screenPoint;
		for (const QPointF& pt : *pts) {
			if (clipPt(pt, m_visibleRect)) {	// 1. 裁剪数据（点，直接过滤）
				screenPoint = dataToScreen(pt);
				// 绘制9个点，加大点
				painter.drawRect(QRectF(screenPoint.x() - 1, screenPoint.y() - 1, 3, 3));
			}
		}
	}
	painter.restore();
}
void Diagram::drawPolygons(std::vector<LineUI>* lines, PolygonStyle* s, QPainter& painter) {
	if (lines->empty()) return;
	// 设置多边形填充(暂未实现)
	if (s->m_fill) {
		painter.setBrush(QBrush(s->m_fillColor));
	}
	else {
		painter.setBrush(Qt::NoBrush);
	}
	// 设置外框轮廓
	// 设置线条属性
	QPen pen(s->m_outlineColor);		// 设置颜色
	pen.setWidthF(s->m_outlineWidth);   // 设置线宽
	QPen penPolyLine = pen;
	if (s->m_outlineShow) {
		float width = lines->at(0).width * m_scaleFactor;
		if (width > 1 && width < 400) {
			penPolyLine.setWidthF(width);
			painter.setPen(penPolyLine);
		}
		else
			painter.setPen(pen);
	}
	else
		painter.setPen(Qt::NoPen);

	// 遍历所有多边形线段并绘制（下面这种绘制多边形的方法暂时不能实现填充）
	for (LineUI line : *lines) {	// 不能用引用，clipLine会对line进行裁剪
		// 1. 裁剪线段（这里需要实现 clipLine 函数）
		if (clipLine(line, m_visibleRect)) {
			// 2. 转换为屏幕坐标并绘制
			QLineF screenLine(dataToScreen(line.p1()), dataToScreen(line.p2()));
			painter.drawLine(screenLine);
		}
	}
}
void Diagram::drawLinePoints(std::vector<LineUI>* lines, PointStyle* s, QPainter& painter) {
	// 设置端点属性
	if (s->m_fill) 			// 设置填充
		painter.setBrush(QBrush(s->m_fillColor));
	else
		painter.setBrush(Qt::NoBrush);

	if (s->m_outlineShow) {	// 设置外框
		QPen pen(s->m_outlineColor);
		pen.setWidthF(s->m_outlineWidth);
		painter.setPen(pen);
	}
	else
		painter.setPen(Qt::NoPen);
	for (const LineUI& line : *lines) {
		QPointF pt1 = line.p1(), pt2 = line.p2();
		if (clipPt(pt1, m_visibleRect)) {	// 1. 裁剪数据（点，直接过滤）
			painter.drawEllipse(dataToScreen(pt1), s->m_r, s->m_r);// 绘制椭圆
		}
		if (clipPt(pt2, m_visibleRect)) {	// 1. 裁剪数据（点，直接过滤）
			painter.drawEllipse(dataToScreen(pt2), s->m_r, s->m_r);// 绘制椭圆
		}
	}
}
void Diagram::drawCircles(std::vector<CircleUI>* circles, PolygonStyle* s, QPainter& painter) {
	if (circles->empty()) return;
	if (s->m_fill) 			// 设置填充
		painter.setBrush(QBrush(s->m_fillColor));
	else
		painter.setBrush(Qt::NoBrush);

	if (s->m_outlineShow) {	// 设置外框
		QPen pen(s->m_outlineColor);
		pen.setWidthF(s->m_outlineWidth);
		painter.setPen(pen);
	}
	else
		painter.setPen(Qt::NoPen);
	float radius = 1;
	for (const CircleUI& cc : *circles) {
		if (clipPt(cc, m_visibleRect)) {	// 1. 裁剪数据（点，直接过滤）
			radius = cc.radius * m_scaleFactor;
			if (radius > 200)
				radius = 2;
			painter.drawEllipse(dataToScreen(cc), radius, radius);// 绘制椭圆
		}
	}
}

//5.1数据坐标转屏幕坐标
QPointF Diagram::dataToScreen(const QPointF& dataPoint) const {
	return m_transform.map(dataPoint);
}
LineUI Diagram::dataToScreen(const LineUI& dataLine) const {
	return m_transform.map(dataLine);
}
QPointF Diagram::screenToData(const QPointF& screenPoint) const {
	QPointF dataPoint = m_transform.inverted().map(screenPoint);
	return { dataPoint.x(), dataPoint.y() };
}
void Diagram::updateTransform() {
	m_transform = QTransform();
	// 1.应用平移（最后执行的变换）
	m_transform.translate(m_panOffset.x(), m_panOffset.y());

	// 2.应用缩放
	m_transform.scale(m_scaleFactor, m_scaleFactor);
	// 3.旋转与翻转
	if (m_config->m_rotate_90)
		m_transform.rotate(90);
	if (m_config->m_flip_left_right)
		m_transform.scale(-1, 1);
	if (m_config->m_flip_up_down)
		m_transform.scale(1, -1);
}

//5.2线段裁剪
bool Diagram::clipLine(QPointF& p1, QPointF& p2, const QRectF& viewport) const {
	// 使用Liang-Barsky算法裁剪
	qreal x0 = p1.x(), y0 = p1.y();
	qreal x1 = p2.x(), y1 = p2.y();

	qreal t0 = 0.0, t1 = 1.0;
	qreal dx = x1 - x0, dy = y1 - y0;

	// 遍历所有裁剪边界
	for (int edge = 0; edge < 4; ++edge) {
		qreal p = 0, q = 0;

		switch (edge) {
		case 0: p = -dx; q = x0 - viewport.left(); break; // left
		case 1: p = dx; q = viewport.right() - x0; break; // right
		case 2: p = -dy; q = y0 - viewport.top(); break; // top
		case 3: p = dy; q = viewport.bottom() - y0; break; // bottom
		}

		if (p == 0) {
			if (q < 0) return false; // 线段平行且在裁剪区域外
		}
		else {
			qreal r = q / p;
			if (p < 0) {
				if (r > t1) return false;
				if (r > t0) t0 = r;
			}
			else {
				if (r < t0) return false;
				if (r < t1) t1 = r;
			}
		}
	}
	p1.setX(x0 + t0 * dx);
	p1.setY(y0 + t0 * dy);
	p2.setX(x0 + t1 * dx);
	p2.setY(y0 + t1 * dy);
	return true;
}
bool Diagram::clipLine(LineUI& line, const QRectF& viewport) const {
	// 使用Liang-Barsky算法裁剪
	qreal x0 = line.p1().x(), y0 = line.p1().y();
	qreal x1 = line.p2().x(), y1 = line.p2().y();

	qreal t0 = 0.0, t1 = 1.0;
	qreal dx = x1 - x0, dy = y1 - y0;

	// 遍历所有裁剪边界
	for (int edge = 0; edge < 4; ++edge) {
		qreal p = 0, q = 0;

		switch (edge) {
		case 0: p = -dx; q = x0 - viewport.left(); break; // left
		case 1: p = dx; q = viewport.right() - x0; break; // right
		case 2: p = -dy; q = y0 - viewport.top(); break; // top
		case 3: p = dy; q = viewport.bottom() - y0; break; // bottom
		}

		if (p == 0) {
			if (q < 0) return false; // 线段平行且在裁剪区域外
		}
		else {
			qreal r = q / p;
			if (p < 0) {
				if (r > t1) return false;
				if (r > t0) t0 = r;
			}
			else {
				if (r < t0) return false;
				if (r < t1) t1 = r;
			}
		}
	}
	line.setLine(x0 + t0 * dx, y0 + t0 * dy, x0 + t1 * dx, y0 + t1 * dy);
	return true;
}
bool Diagram::clipPt(const QPointF& p1, const QRectF& viewport) const {
	if (p1.x() < viewport.left() || p1.x() > viewport.right() ||
		p1.y() < viewport.top() || p1.y() > viewport.bottom()) {
		return false;
	}
	return true;
}


//########################## 事件管理（保护） ##########################

//1.综合事件：【初始化显示】，【resize】，【定时刷新】，【绘图事件】
void Diagram::showEvent(QShowEvent* event) {
	//当控件首次显示或从隐藏状态变为可见状态时，自动调用该函数，一般用于执行初始化操作
	QWidget::showEvent(event);
}
void Diagram::resizeEvent(QResizeEvent* event) {
	QWidget::resizeEvent(event);
	//initViewTransform();

	if (!m_initialized && width() > 10 && height() > 10) {
		m_initialized = true;
		initViewTransform(); // 正确计算 m_scaleFactor
	}
	if (m_initialized)
		update();
}
void Diagram::timerEvent(QTimerEvent* event) {
	if (event->timerId() == m_moveTimer.timerId()) {
		m_moveTimer.stop(); // 单次触发
		//moveRun();
	}
}
void Diagram::paintEvent(QPaintEvent* event) {
	QPainter painter(this);
	m_config->m_r = m_scaleFactor >= 5 ? m_scaleFactor / 3 : 2;	//绘制实心点的半径
	//2.绘图
	if (m_cacheValid) {		//利用缓存快速绘制
		painter.drawPixmap(m_cacheOffset, m_canvasCache);
	}
	else {					// 正常绘制流程
		paintVisibleContent(event->rect(), painter);
	}
	if (m_slLine.showInfo && !m_slLine.infoText.isEmpty())
		paintContent(painter);

}

//2.鼠标事件
void Diagram::mousePressEvent(QMouseEvent* event) {
	QPointF clickPt = screenToData(event->pos());
	m_pressPos = event->pos();
	if (event->button() == Qt::LeftButton) {
		m_config->painState = PUSH_LINE;
		m_targetSelected = selectTargetBegin(clickPt);
		if (m_targetSelected)
			emit leftPressed(&m_slLine);
	}
	else if (event->button() == Qt::MiddleButton) {
		m_config->painState = MOVE_CANVAS;	//移动画布
		setCursor(Qt::ClosedHandCursor); // 改变光标形状
		moveCanvasBegin(clickPt);		//生成缓存区
	}
}
void Diagram::mouseMoveEvent(QMouseEvent* event) {
	m_movePos = event->pos();
	if (!m_moveTimer.isActive()) {
		m_moveTimer.start(20, this);
	}
	if (m_config->painState == PUSH_LINE) {
		if (m_targetSelected)
			selectTargetActivate();
	}
	else if (m_config->painState == MOVE_CANVAS) {
		moveCanvasActivate();		//持续更新，绘制移动画布
	}
	//悬停检测
	if (m_config->painState == NONE) {
		QPointF hoverPt = screenToData(event->pos());
		checkHoverTarget(hoverPt);
	}
}
void Diagram::mouseReleaseEvent(QMouseEvent* event) {
	m_releasePos = event->pos();
	if (m_config->painState == PUSH_LINE) {
		if (m_slLine.lineSelected || m_slLine.pinSelected) {
			selectTargetEnd(screenToData(m_releasePos));
		}
	}
	else if (m_config->painState == MOVE_CANVAS) {
		setCursor(Qt::ArrowCursor); // 恢复光标形状
		moveCanvasEnd(m_releasePos);
	}
	if (event->button() == Qt::LeftButton) {
		m_config->painState = NONE;
	}
	else if (event->button() == Qt::MiddleButton) {
		m_config->painState = NONE;
	}
}
void Diagram::leaveEvent(QEvent* event) {
	QWidget::leaveEvent(event);

	// 当鼠标离开窗口时，清除鼠标信息
	if (m_slLine.showInfo) {
		m_slLine.clear();
		m_slLine.showInfo = false;
		update(); // 触发重绘，清除信息框
	}
}
void Diagram::wheelEvent(QWheelEvent* event) {
	if (m_config->painState == NONE) {
		int delta = event->angleDelta().y();		//鼠标滚动的度数
		QPointF mouseScenePos = event->position();	//浮点坐标
		wheelEventRun(delta, mouseScenePos);
		event->accept(); // 阻止事件继续传播
	}
}
//3.鼠标事件执行逻辑


//滚动滚轮时，执行的内容
void Diagram::wheelEventRun(const int& delta, const QPointF& screenPos) {
	// 1. 获取鼠标在数据坐标系中的位置
	QPointF mouseDataPos = m_transform.inverted().map(screenPos);

	// 2. 计算缩放因子（非线性缩放更平滑）
	const qreal zoomFactor = 1.1; // 每次滚轮步进的缩放系数
	qreal scaleFactor = (delta > 0) ? zoomFactor : 1.0 / zoomFactor;

	// 3. 限制缩放范围
	qreal newScale = m_scaleFactor * scaleFactor;
	newScale = qBound(0.0001, newScale, 10000.0); // 限制在0.001~1000倍之间

	// 4. 计算保持鼠标点不变的平移量
	QPointF mousePosBefore = m_transform.map(mouseDataPos);
	m_scaleFactor = newScale;
	updateTransform(); // 先更新变换矩阵
	QPointF mousePosAfter = m_transform.map(mouseDataPos);
	m_panOffset += mousePosBefore - mousePosAfter;

	// 5. 应用新变换
	updateTransform();
	update();
}

//########################## 事件执行（私有） ##########################

//鼠标悬停时，捕捉点或者线，显示坐标和线长
void Diagram::checkHoverTarget(const QPointF& hoverPt) {
	SelectedTarget tempSlLine = m_slLine;	// 保存当前的选择状态
	m_slLine.clear();
	m_slLine.showInfo = false; // 保持信息显示状态独立

	bool foundTarget =		// 前景层、背景层检测，先检测点
		selectTarget(hoverPt, true, m_pts2, m_ss_pts2) ||
		selectTarget(hoverPt, true, m_linePts2, m_ss_linePts2) ||
		selectTarget(hoverPt, true, m_pts1, m_ss_pts1) ||
		selectTarget(hoverPt, true, m_linePts1, m_ss_linePts1) ||
		selectTarget(hoverPt, true, m_lines2, m_ss_lines2) ||
		selectTarget(hoverPt, true, m_polys2, m_ss_polys2) ||
		selectTarget(hoverPt, true, m_lines1, m_ss_lines1) ||
		selectTarget(hoverPt, true, m_polys1, m_ss_polys1);

	if (foundTarget) {
		// 保持信息显示，但清除选择状态（避免影响实际鼠标操作）
		m_slLine.pinSelected = false;
		m_slLine.lineSelected = false;
		m_slLine.isMoving = false;
		m_slLine.linesToRun = nullptr;
	}
	else {
		// 没有捕捉到对象，显示鼠标当前位置坐标
		m_slLine.infoText = QString("%1, %2")
			.arg(hoverPt.x(), 0, 'f', 2)
			.arg(hoverPt.y(), 0, 'f', 2);
		m_slLine.infoPosition = hoverPt;
		m_slLine.showInfo = true;

	}
	update();
	if (tempSlLine.pinSelected || tempSlLine.lineSelected) {
		m_slLine = tempSlLine;
	}
}
//鼠标左键，捕捉点或线，按下捕捉，释放返回偏移量和捕捉到的信息
bool Diagram::selectTargetBegin(const QPointF& clickPt) {
	if (
		selectTarget(clickPt, false, m_pts2, m_ss_pts2) ||		// 前景层
		selectTarget(clickPt, false, m_lines2, m_ss_lines2) ||
		selectTarget(clickPt, false, m_polys2, m_ss_polys2) ||
		selectTarget(clickPt, false, m_linePts2, m_ss_linePts2) ||
		selectTarget(clickPt, false, m_pts1, m_ss_pts1) ||		// 背景层
		selectTarget(clickPt, false, m_lines1, m_ss_lines1) ||
		selectTarget(clickPt, false, m_polys1, m_ss_polys1) ||
		selectTarget(clickPt, false, m_linePts1, m_ss_linePts1)
		) {
		return true;
	}
	return false;
}
bool Diagram::selectTargetActivate() {
	m_slLine.isMoving = true;
	QPointF currentPtPos = screenToData(m_movePos);
	m_slLine.offset = currentPtPos - m_slLine.nearistPt;
	emit leftPressedMove(&m_slLine);
	update();
	return false;
}
bool Diagram::selectTargetEnd(const QPointF& releasePt) {
	m_slLine.isMoving = false;
	m_slLine.offset = releasePt - m_slLine.nearistPt;
	emit leftReleaseed(&m_slLine);
	m_slLine.clear();
	update();
	return false;
}
//鼠标中键，移动画布
void Diagram::moveCanvasBegin(const QPointF& clickPt) {
	m_canvasCache = grab(this->rect());
	m_cacheValid = true;
}
void Diagram::moveCanvasActivate() {
	if (!m_cacheValid) return;
	m_cacheOffset = m_movePos - m_pressPos;
	update();
}
void Diagram::moveCanvasEnd(const QPoint& releasePos) {
	// 计算实际偏移量（数据坐标系）
	m_panOffset += releasePos - m_pressPos;
	updateTransform();// 更新实际变换
	m_cacheValid = false;
	update();
	m_canvasCache = QPixmap(); // 清除缓存
}

// 吸附阈值（数据坐标系）
template<typename DataType, typename StyleType>
bool Diagram::selectTarget(const QPointF& posPt, const bool& hover, std::vector<DataType*>& dataVector, std::vector<StyleType*>& styleVector) {
	for (int i = 0; i < styleVector.size(); i++) {
		if (setSelections(posPt, hover, dataVector[i], styleVector[i])) {
			return true;
		}
	}
	return false;
}
bool Diagram::setSelections(const QPointF& clickPt, const bool& hover, std::vector<QPointF>* candidates, PointStyle* s) {
	if (s->m_type <= 0 || !s->isVisible()) {
		return false;
	}
	double snapThreshold = m_snapThresholdPixels / m_scaleFactor;
	if (s->m_type <= 1000 || (s->m_type > 2000 && s->m_type <= 3000) || (s->m_type == 6001 && hover)) {	// 都可选择
		//将m_wires设置为推挤线
		bool selected = setSelectedPt(clickPt, *candidates, snapThreshold);
		if (selected) return true;
	}
	else {
		return false;
	}
	return false;
}
bool Diagram::setSelections(const QPointF& clickPt, const bool& hover, std::vector<LineUI>* candidates, LineStyle* s) {
	if (s->m_type <= 0 || !s->isVisible()) {
		return false;
	}
	double snapThreshold = m_snapThresholdPixels / m_scaleFactor;
	bool selected = false;
	if (s->m_type <= 1000 || (s->m_type == 6001 && hover)) {		// 都可选择
		selected = setSelectedPt(clickPt, *candidates, snapThreshold);
		if (selected) return true;
		else selected = setSelectedLine(clickPt, *candidates, snapThreshold);
		if (selected) return true;
	}
	else if (s->m_type <= 2000) {	// 可选择线段
		selected = setSelectedLine(clickPt, *candidates, snapThreshold);
		if (selected) return true;
	}
	else if (s->m_type <= 3000) {	// 只可选择端点
		selected = setSelectedPt(clickPt, *candidates, snapThreshold);
		if (selected) return true;
	}
	else {
		return false;
	}
	return false;
}
bool Diagram::setSelections(const QPointF& clickPt, const bool& hover, std::vector<LineUI>* candidates, PolygonStyle* s) {
	if (s->m_type <= 0 || !s->isVisible()) {
		return false;
	}
	double snapThreshold = m_snapThresholdPixels / m_scaleFactor;
	bool selected = false;
	if (s->m_type <= 1000 || (s->m_type == 6001 && hover)) {		// 都可选择
		selected = setSelectedPt(clickPt, *candidates, snapThreshold);
		if (selected) return true;
		else selected = setSelectedLine(clickPt, *candidates, snapThreshold);
		if (selected) return true;
	}
	else if (s->m_type <= 2000) {	// 可选择线段
		selected = setSelectedLine(clickPt, *candidates, snapThreshold);
		if (selected) return true;
	}
	else if (s->m_type <= 3000) {	// 只可选择端点
		selected = setSelectedPt(clickPt, *candidates, snapThreshold);
		if (selected) return true;
	}
	else {
		return false;
	}
	return false;
}
bool Diagram::setSelections(const QPointF& clickPt, const bool& hover, std::vector<LineUI>* candidates, PointStyle* s) {
	if (s->m_type <= 0 || !s->isVisible()) {
		return false;
	}
	double snapThreshold = m_snapThresholdPixels / m_scaleFactor;
	bool selected = false;
	if (s->m_type <= 1000 || (s->m_type > 2000 && s->m_type <= 3000) || (s->m_type == 6001 && hover)) {		// 都可选择
		selected = setSelectedPt(clickPt, *candidates, snapThreshold);
		if (selected) return true;
	}
	else {
		return false;
	}
	return false;
}

bool Diagram::setSelectedPt(const QPointF& clickPt, const vector<QPointF>& line, double snapThreshold) {
	qreal pickPointTolerance = m_config->m_r / m_scaleFactor;		//转化为数据坐标系（点的半径）
	pickPointTolerance *= 0.6;									// 缩小吸附范围，点视觉半径的0.8倍
	pickPointTolerance = pickPointTolerance > snapThreshold ? pickPointTolerance : snapThreshold;
	for (const QPointF& pt : line) {
		if (LineUI(clickPt, pt).length() < pickPointTolerance) {
			m_slLine.pinSelected = true;
			m_slLine.nearistPt = pt;
			// 屏幕显示信息
			m_slLine.infoText = QString("p ( %1, %2 )").arg(pt.x(), 0, 'f', 2).arg(pt.y(), 0, 'f', 2);
			m_slLine.infoPosition = pt;
			m_slLine.showInfo = true;
			return true;
		}
	}
	return false;
}
bool Diagram::setSelectedPt(const QPointF& clickPt, const LineUI& line, double snapThreshold) {
	qreal pickPointTolerance = m_config->m_r / m_scaleFactor;		//转化为数据坐标系（点的半径）
	pickPointTolerance *= 0.6;									// 缩小吸附范围，点视觉半径的0.8倍
	pickPointTolerance = pickPointTolerance > snapThreshold ? pickPointTolerance : snapThreshold;
	if (LineUI(clickPt, line.p1()).length() < pickPointTolerance) {
		m_slLine.pinSelected = true;
		m_slLine.nearistPt = line.p1();
		// 屏幕显示信息
		m_slLine.infoText = QString("p1 ( %1, %2 )").arg(line.p1().x(), 0, 'f', 2).arg(line.p1().y(), 0, 'f', 2);
		m_slLine.infoPosition = line.p1();
		m_slLine.showInfo = true;
		return true;
	}
	else if (LineUI(clickPt, line.p2()).length() < pickPointTolerance) {
		m_slLine.pinSelected = true;
		m_slLine.nearistPt = line.p2();
		// 屏幕显示信息
		m_slLine.infoText = QString("p2 ( %1, %2 )").arg(line.p2().x(), 0, 'f', 2).arg(line.p2().y(), 0, 'f', 2);
		m_slLine.infoPosition = line.p2();
		m_slLine.showInfo = true;
		return true;
	}
	return false;
}
bool Diagram::setSelectedPt(const QPointF& clickPt, vector<LineUI>& lines, double snapThreshold) {
	for (auto& line : lines) {
		setSelectedPt(clickPt, line, snapThreshold);
		if (m_slLine.pinSelected) {
			m_slLine.linesToRun = &lines;
			return true;
		}
	}
	return false;
}
bool Diagram::setSelectedPt(const QPointF& clickPt, vector<vector<LineUI>>& lines, double snapThreshold) {
	for (auto& line : lines) {
		setSelectedPt(clickPt, line, snapThreshold);
		if (m_slLine.pinSelected) {
			return true;
		}
	}
	return false;
}
bool Diagram::setSelectedPt(const QPointF& clickPt, vector<vector<vector<LineUI>>>& lines, double snapThreshold) {
	for (auto& line : lines) {
		setSelectedPt(clickPt, line, snapThreshold);
		if (m_slLine.pinSelected) {
			return true;
		}
	}
	return false;
}

bool Diagram::setSelectedLine(const QPointF& clickPt, const LineUI& line, double snapThreshold) {
	const QPointF p1 = line.p1();
	const QPointF p2 = line.p2();
	// 处理零长度线段
	if (p1 == p2) {
		if (sqrt(pow(clickPt.x() - p1.x(), 2) + pow(clickPt.y() - p1.y(), 2)) < const_minValue);
		return false;
	}
	// 向量计算
	const QPointF AP = clickPt - p1;
	const QPointF AB = p2 - p1;
	const double dotProduct = AP.x() * AB.x() + AP.y() * AB.y();
	const double abLengthSquared = AB.x() * AB.x() + AB.y() * AB.y();
	const double ratio = qBound(0.0, dotProduct / abLengthSquared, 1.0);
	// 计算最近点
	const QPointF nearestPoint = p1 + ratio * AB;
	const double distance = sqrt(pow(clickPt.x() - nearestPoint.x(), 2) + pow(clickPt.y() - nearestPoint.y(), 2));
	//自动吸附线逻辑
	qreal pinMinDist = m_config->m_r / m_scaleFactor;
	if (distance < snapThreshold) {
		m_slLine.lineSelected = true;
		m_slLine.sLine = line;
		m_slLine.nearistPt = nearestPoint;
		// 屏幕显示信息
		double lineLength = sqrt(abLengthSquared);
		m_slLine.infoText = QString("p1 ( %1, %2 )\np2 ( %3, %4 )\nlen: %5")
			.arg(p1.x(), 0, 'f', 2).arg(p1.y(), 0, 'f', 2)
			.arg(p2.x(), 0, 'f', 2).arg(p2.y(), 0, 'f', 2)
			.arg(lineLength, 0, 'f', 2);
		m_slLine.infoPosition = nearestPoint;
		m_slLine.showInfo = true;
		return true;
	}
	return false;
}
bool Diagram::setSelectedLine(const QPointF& clickPt, vector<LineUI>& lines, double snapThreshold) {
	for (auto& line : lines) {
		setSelectedLine(clickPt, line, snapThreshold);
		if (m_slLine.lineSelected) {
			m_slLine.linesToRun = &lines;
			return true;
		}
	}
	return false;
}
bool Diagram::setSelectedLine(const QPointF& clickPt, vector<vector<LineUI>>& lines, double snapThreshold) {
	for (auto& line : lines) {
		setSelectedLine(clickPt, line, snapThreshold);
		if (m_slLine.lineSelected) {
			return true;
		}
	}
	return false;
}
bool Diagram::setSelectedLine(const QPointF& clickPt, vector<vector<vector<LineUI>>>& lines, double snapThreshold) {
	for (auto& line : lines) {
		setSelectedLine(clickPt, line, snapThreshold);
		if (m_slLine.lineSelected) {
			return true;
		}
	}
	return false;
}
