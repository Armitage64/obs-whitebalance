/*
obs-whitebalance
Copyright (C) 2026 obs-whitebalance contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include <QDialog>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QWidget>

#include <obs.h>

#include "wb-math.h"

struct wb_filter;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QTimer;

/* Shows the captured frame, a loupe under the cursor and the sample box. */
class PickerView : public QWidget {
	Q_OBJECT

public:
	explicit PickerView(QWidget *parent = nullptr);

	void setImage(const QImage &img);
	void setSampleSize(int n);
	void setLastSample(const QRect &r) { lastSample_ = r; update(); }
	void setMessage(const QString &msg);
	/* text banner drawn over the image (empty = none) */
	void setOverlay(const QString &msg);

	/* square of sampleSize centered on p, clipped to the image */
	QRect sampleRectAt(const QPoint &p) const;

signals:
	void hovered(const QRect &imageRect);
	void sampled(const QRect &imageRect);

protected:
	void paintEvent(QPaintEvent *) override;
	void mouseMoveEvent(QMouseEvent *) override;
	void mousePressEvent(QMouseEvent *) override;
	void mouseReleaseEvent(QMouseEvent *) override;
	void leaveEvent(QEvent *) override;

private:
	QRectF imageTarget() const;
	bool widgetToImage(const QPointF &w, QPoint &out) const;
	QRectF imageToWidget(const QRect &r) const;
	QRect dragRect() const;
	void drawLoupe(QPainter &p);

	QImage image_;
	QString message_;
	QString overlay_;
	int sampleSize_ = 5;
	bool hovering_ = false;
	QPoint hoverImg_;
	QPointF hoverWidget_;
	bool dragging_ = false;
	QPoint dragStart_;
	QRect lastSample_;
};

class PickerDialog : public QDialog {
	Q_OBJECT

public:
	PickerDialog(obs_source_t *filter, wb_filter *f, QWidget *parent);
	~PickerDialog() override;

	void receiveFrame(const uint8_t *rgba, uint32_t w, uint32_t h, uint32_t linesize);

protected:
	void reject() override;

private slots:
	void poll();
	void onHovered(const QRect &r);
	void onSampled(const QRect &r);
	void onSpinChanged();
	void onSampleSizeChanged();
	void onReset();

private:
	struct wb_rgb averageLinear(const QRect &r) const;
	void applyToFilter(double temperature, double tint);
	void refreshDisplay();
	void setStatus(const QString &text, bool warning);
	void checkFrameContent();

	obs_source_t *filter_; /* strong ref held for the dialog's lifetime */
	wb_filter *f_;
	uint64_t seq_ = 0;
	int framesSeen_ = 0;
	int pollsWithoutFrame_ = 0;

	double origTemp_ = 0, origTint_ = 0;
	int origSampleSize_ = 5;

	QImage frame_; /* uncorrected, RGBA8888 non-linear sRGB */
	float decodeLut_[256];
	uint8_t encodeLut_[4096];

	PickerView *view_;
	QLabel *readout_;
	QLabel *status_;
	QCheckBox *showCorrected_;
	QComboBox *sampleSize_;
	QDoubleSpinBox *temp_;
	QDoubleSpinBox *tint_;
	QTimer *timer_;
};
