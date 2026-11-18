/*
obs-whitebalance
Copyright (C) 2026 obs-whitebalance contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "picker-dialog.hpp"
#include "wb-filter.h"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

static QString T(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

/* ========================================================================= */
/* PickerView                                                                */

PickerView::PickerView(QWidget *parent) : QWidget(parent)
{
	setMouseTracking(true);
	setCursor(Qt::CrossCursor);
	setMinimumSize(480, 270);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void PickerView::setImage(const QImage &img)
{
	image_ = img;
	update();
}

void PickerView::setSampleSize(int n)
{
	sampleSize_ = std::max(1, n);
	update();
}

void PickerView::setMessage(const QString &msg)
{
	message_ = msg;
	update();
}

void PickerView::setOverlay(const QString &msg)
{
	if (overlay_ == msg)
		return;
	overlay_ = msg;
	update();
}

QRectF PickerView::imageTarget() const
{
	if (image_.isNull())
		return QRectF();
	const double s = std::min(width() / (double)image_.width(), height() / (double)image_.height());
	const double w = image_.width() * s, h = image_.height() * s;
	return QRectF((width() - w) / 2.0, (height() - h) / 2.0, w, h);
}

bool PickerView::widgetToImage(const QPointF &w, QPoint &out) const
{
	const QRectF t = imageTarget();
	if (t.isEmpty() || !t.contains(w))
		return false;
	const int x = (int)std::floor((w.x() - t.x()) * image_.width() / t.width());
	const int y = (int)std::floor((w.y() - t.y()) * image_.height() / t.height());
	out = QPoint(std::clamp(x, 0, image_.width() - 1), std::clamp(y, 0, image_.height() - 1));
	return true;
}

QRectF PickerView::imageToWidget(const QRect &r) const
{
	const QRectF t = imageTarget();
	if (t.isEmpty())
		return QRectF();
	const double sx = t.width() / image_.width(), sy = t.height() / image_.height();
	return QRectF(t.x() + r.x() * sx, t.y() + r.y() * sy, r.width() * sx, r.height() * sy);
}

QRect PickerView::sampleRectAt(const QPoint &p) const
{
	const int half = sampleSize_ / 2;
	return QRect(p.x() - half, p.y() - half, sampleSize_, sampleSize_).intersected(image_.rect());
}

QRect PickerView::dragRect() const
{
	return QRect(dragStart_, hoverImg_).normalized().intersected(image_.rect());
}

void PickerView::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.fillRect(rect(), QColor(24, 24, 24));

	if (image_.isNull()) {
		p.setPen(QColor(200, 200, 200));
		p.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, message_);
		return;
	}

	p.setRenderHint(QPainter::SmoothPixmapTransform, true);
	p.drawImage(imageTarget(), image_);

	if (!overlay_.isEmpty()) {
		const QRect box = rect().adjusted(40, 0, -40, 0);
		QRect text = p.fontMetrics().boundingRect(box, Qt::AlignCenter | Qt::TextWordWrap, overlay_);
		text.adjust(-14, -10, 14, 10);
		p.fillRect(text, QColor(0, 0, 0, 200));
		p.setPen(QColor(232, 163, 61));
		p.drawText(box, Qt::AlignCenter | Qt::TextWordWrap, overlay_);
	}

	p.setRenderHint(QPainter::Antialiasing, false);
	if (!lastSample_.isNull()) {
		p.setPen(QPen(QColor(255, 210, 0), 1, Qt::DashLine));
		p.setBrush(Qt::NoBrush);
		p.drawRect(imageToWidget(lastSample_));
	}

	if (dragging_) {
		p.setPen(QPen(Qt::white, 1));
		p.drawRect(imageToWidget(dragRect()));
		p.setPen(QPen(Qt::black, 1, Qt::DotLine));
		p.drawRect(imageToWidget(dragRect()));
	} else if (hovering_) {
		p.setPen(QPen(Qt::white, 1));
		p.drawRect(imageToWidget(sampleRectAt(hoverImg_)));
		drawLoupe(p);
	}
}

void PickerView::drawLoupe(QPainter &p)
{
	const int span = std::max(15, sampleSize_ + 6) | 1; /* odd, image pixels */
	const int size = 96;                               /* widget pixels */
	const double gap = 14;                             /* offset from cursor */
	const QRect src(hoverImg_.x() - span / 2, hoverImg_.y() - span / 2, span, span);

	/* build the zoomed region with edge clamping */
	QImage crop(span, span, QImage::Format_RGBA8888);
	crop.fill(Qt::black);
	{
		QPainter cp(&crop);
		cp.drawImage(QPoint(0, 0), image_, src);
	}

	QPointF pos = hoverWidget_ + QPointF(gap, gap);
	if (pos.x() + size > width())
		pos.setX(hoverWidget_.x() - gap - size);
	if (pos.y() + size > height())
		pos.setY(hoverWidget_.y() - gap - size);
	const QRectF dst(pos, QSizeF(size, size));

	p.setRenderHint(QPainter::SmoothPixmapTransform, false);
	p.drawImage(dst, crop);

	const double cell = size / (double)span;
	const int half = sampleSize_ / 2;
	const QRectF box(dst.x() + (span / 2 - half) * cell, dst.y() + (span / 2 - half) * cell, sampleSize_ * cell,
			 sampleSize_ * cell);
	p.setPen(QPen(Qt::white, 1));
	p.drawRect(box);
	p.setPen(QPen(Qt::black, 1));
	p.drawRect(box.adjusted(-1, -1, 1, 1));
	p.setPen(QPen(QColor(255, 255, 255, 200), 2));
	p.drawRect(dst);
}

void PickerView::mouseMoveEvent(QMouseEvent *e)
{
	QPoint ip;
	hoverWidget_ = e->position();
	if (widgetToImage(e->position(), ip)) {
		hovering_ = true;
		hoverImg_ = ip;
		emit hovered(dragging_ ? dragRect() : sampleRectAt(ip));
	} else if (!dragging_) {
		hovering_ = false;
	}
	update();
}

void PickerView::mousePressEvent(QMouseEvent *e)
{
	if (e->button() != Qt::LeftButton)
		return;
	QPoint ip;
	if (!widgetToImage(e->position(), ip))
		return;
	dragging_ = true;
	dragStart_ = ip;
	hoverImg_ = ip;
	update();
}

void PickerView::mouseReleaseEvent(QMouseEvent *e)
{
	if (e->button() != Qt::LeftButton || !dragging_)
		return;
	dragging_ = false;

	QPoint ip;
	if (widgetToImage(e->position(), ip))
		hoverImg_ = ip;

	const QRect r = dragRect();
	const bool isClick = r.width() <= 3 && r.height() <= 3;
	const QRect sample = isClick ? sampleRectAt(dragStart_) : r;
	if (!sample.isEmpty()) {
		lastSample_ = sample;
		emit sampled(sample);
	}
	update();
}

void PickerView::leaveEvent(QEvent *)
{
	if (!dragging_) {
		hovering_ = false;
		update();
	}
}

/* ========================================================================= */
/* PickerDialog                                                              */

static void frame_cb(void *param, const uint8_t *rgba, uint32_t w, uint32_t h, uint32_t linesize)
{
	static_cast<PickerDialog *>(param)->receiveFrame(rgba, w, h, linesize);
}

PickerDialog::PickerDialog(obs_source_t *filter, wb_filter *f, QWidget *parent)
	: QDialog(parent),
	  filter_(obs_source_get_ref(filter)),
	  f_(f)
{
	setWindowTitle(T("WhiteBalance.Picker.Title"));
	resize(1000, 680);

	for (int i = 0; i < 256; i++)
		decodeLut_[i] = (float)wb_srgb_to_linear(i / 255.0);
	for (int i = 0; i < 4096; i++)
		encodeLut_[i] = (uint8_t)std::lround(wb_linear_to_srgb(i / 4095.0) * 255.0);

	obs_data_t *s = obs_source_get_settings(filter_);
	origTemp_ = obs_data_get_double(s, S_TEMPERATURE);
	origTint_ = obs_data_get_double(s, S_TINT);
	origSampleSize_ = (int)obs_data_get_int(s, S_SAMPLE_SIZE);
	obs_data_release(s);

	auto *help = new QLabel(T("WhiteBalance.Picker.Help"));
	help->setWordWrap(true);

	view_ = new PickerView(this);
	view_->setSampleSize(origSampleSize_);
	view_->setMessage(T("WhiteBalance.Picker.Waiting"));

	readout_ = new QLabel(" ");
	readout_->setTextInteractionFlags(Qt::TextSelectableByMouse);
	readout_->setFont(QFont(QStringLiteral("Menlo")));
	status_ = new QLabel(" ");
	status_->setWordWrap(true);

	showCorrected_ = new QCheckBox(T("WhiteBalance.Picker.ShowCorrected"));
	showCorrected_->setChecked(true);

	sampleSize_ = new QComboBox;
	for (int n : {1, 3, 5, 9, 15, 31})
		sampleSize_->addItem(QStringLiteral("%1 x %1 px").arg(n), n);
	int idx = sampleSize_->findData(origSampleSize_);
	sampleSize_->setCurrentIndex(idx >= 0 ? idx : 2);

	auto makeSpin = [](double v) {
		auto *sb = new QDoubleSpinBox;
		sb->setRange(WB_PARAM_MIN, WB_PARAM_MAX);
		sb->setDecimals(1);
		sb->setSingleStep(0.5);
		sb->setValue(v);
		sb->setKeyboardTracking(false);
		return sb;
	};
	temp_ = makeSpin(origTemp_);
	tint_ = makeSpin(origTint_);
	temp_->setToolTip(T("WhiteBalance.Temperature.Desc"));
	tint_->setToolTip(T("WhiteBalance.Tint.Desc"));

	auto *controls = new QFormLayout;
	controls->addRow(T("WhiteBalance.SampleSize"), sampleSize_);
	controls->addRow(T("WhiteBalance.Temperature"), temp_);
	controls->addRow(T("WhiteBalance.Tint"), tint_);
	controls->addRow(QString(), showCorrected_);

	sampleSize_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	sampleSize_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	controls->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

	/* Live readout: one line above the image. Its text changes on every
	 * mouse move, so it must never influence the layout: fixed height and
	 * an ignored horizontal size hint (long text is clipped, not grown). */
	readout_->setWordWrap(false);
	readout_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
	readout_->setFixedHeight(readout_->fontMetrics().lineSpacing() + 6);

	auto *left = new QVBoxLayout;
	left->addWidget(readout_);
	left->addWidget(view_, 1);

	/* Fixed-width side panel so status text can't resize the image view. */
	auto *sidePanel = new QWidget;
	sidePanel->setFixedWidth(260);
	auto *side = new QVBoxLayout(sidePanel);
	side->setContentsMargins(0, 0, 0, 0);
	side->addLayout(controls);
	side->addWidget(status_);
	side->addStretch(1);

	auto *body = new QHBoxLayout;
	body->addLayout(left, 1);
	body->addWidget(sidePanel);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset);
	buttons->button(QDialogButtonBox::Reset)->setText(T("WhiteBalance.Reset"));

	auto *root = new QVBoxLayout(this);
	root->addWidget(help);
	root->addLayout(body, 1);
	root->addWidget(buttons);

	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, &PickerDialog::onReset);
	connect(view_, &PickerView::hovered, this, &PickerDialog::onHovered);
	connect(view_, &PickerView::sampled, this, &PickerDialog::onSampled);
	connect(temp_, &QDoubleSpinBox::valueChanged, this, &PickerDialog::onSpinChanged);
	connect(tint_, &QDoubleSpinBox::valueChanged, this, &PickerDialog::onSpinChanged);
	connect(sampleSize_, &QComboBox::currentIndexChanged, this, &PickerDialog::onSampleSizeChanged);
	connect(showCorrected_, &QCheckBox::toggled, this, &PickerDialog::refreshDisplay);

	timer_ = new QTimer(this);
	connect(timer_, &QTimer::timeout, this, &PickerDialog::poll);
	wb_filter_request_snapshot(f_);
	timer_->start(66);
}

PickerDialog::~PickerDialog()
{
	timer_->stop();
	obs_source_release(filter_);
}

void PickerDialog::receiveFrame(const uint8_t *rgba, uint32_t w, uint32_t h, uint32_t linesize)
{
	if (frame_.width() != (int)w || frame_.height() != (int)h)
		frame_ = QImage((int)w, (int)h, QImage::Format_RGBA8888);
	for (uint32_t y = 0; y < h; y++)
		memcpy(frame_.scanLine((int)y), rgba + (size_t)linesize * y, (size_t)w * 4);
}

void PickerDialog::poll()
{
	if (wb_filter_copy_snapshot(f_, &seq_, frame_cb, this)) {
		framesSeen_++;
		pollsWithoutFrame_ = 0;
		checkFrameContent();
		refreshDisplay();
	} else if (++pollsWithoutFrame_ == 30 && framesSeen_ == 0) {
		view_->setMessage(T("WhiteBalance.Picker.NoVideo"));
	}
	wb_filter_request_snapshot(f_);
}

/* Detect a frame that carries no picture (e.g. capture card without signal,
 * or a source that only outputs black) and tell the user why the view is
 * black instead of leaving them guessing. */
void PickerDialog::checkFrameContent()
{
	int maxv = 0;
	const int stepX = std::max(1, frame_.width() / 64), stepY = std::max(1, frame_.height() / 36);
	for (int y = 0; y < frame_.height(); y += stepY) {
		const uint8_t *row = frame_.constScanLine(y);
		for (int x = 0; x < frame_.width(); x += stepX) {
			const uint8_t *px = row + (size_t)x * 4;
			maxv = std::max({maxv, (int)px[0], (int)px[1], (int)px[2]});
		}
	}
	const bool black = maxv < 8;

	if (framesSeen_ == 1) {
		const uint8_t *c = frame_.constScanLine(frame_.height() / 2) + (size_t)(frame_.width() / 2) * 4;
		obs_log(LOG_INFO, "eyedropper first frame %dx%d: center RGB %u %u %u, max %d%s", frame_.width(),
			frame_.height(), c[0], c[1], c[2], maxv, black ? " (all black)" : "");
	}
	view_->setOverlay(black ? T("WhiteBalance.Picker.BlackFrame") : QString());
}

void PickerDialog::refreshDisplay()
{
	if (frame_.isNull())
		return;
	if (!showCorrected_->isChecked()) {
		view_->setImage(frame_);
		return;
	}

	const struct wb_rgb g = wb_gains_from_params(temp_->value(), tint_->value());
	const float gr = (float)g.r * 4095.0f, gg = (float)g.g * 4095.0f, gb = (float)g.b * 4095.0f;

	QImage out(frame_.size(), QImage::Format_RGBA8888);
	for (int y = 0; y < frame_.height(); y++) {
		const uint8_t *src = frame_.constScanLine(y);
		uint8_t *dst = out.scanLine(y);
		for (int x = 0; x < frame_.width(); x++, src += 4, dst += 4) {
			dst[0] = encodeLut_[std::min(4095, (int)(decodeLut_[src[0]] * gr + 0.5f))];
			dst[1] = encodeLut_[std::min(4095, (int)(decodeLut_[src[1]] * gg + 0.5f))];
			dst[2] = encodeLut_[std::min(4095, (int)(decodeLut_[src[2]] * gb + 0.5f))];
			dst[3] = 255;
		}
	}
	view_->setImage(out);
}

struct wb_rgb PickerDialog::averageLinear(const QRect &r) const
{
	struct wb_rgb acc = {0, 0, 0};
	const QRect c = r.intersected(frame_.rect());
	if (c.isEmpty())
		return acc;
	for (int y = c.top(); y <= c.bottom(); y++) {
		const uint8_t *px = frame_.constScanLine(y) + (size_t)c.left() * 4;
		for (int x = c.left(); x <= c.right(); x++, px += 4) {
			acc.r += decodeLut_[px[0]];
			acc.g += decodeLut_[px[1]];
			acc.b += decodeLut_[px[2]];
		}
	}
	const double n = (double)c.width() * c.height();
	acc.r /= n;
	acc.g /= n;
	acc.b /= n;
	return acc;
}

void PickerDialog::onHovered(const QRect &r)
{
	if (frame_.isNull())
		return;
	const struct wb_rgb a = averageLinear(r);
	auto to8 = [](double v) { return (int)std::lround(wb_linear_to_srgb(v) * 255.0); };
	QString text = QStringLiteral("RGB %1 %2 %3     ").arg(to8(a.r), 3).arg(to8(a.g), 3).arg(to8(a.b), 3);
	double t = 0, m = 0;
	switch (wb_params_from_sample(a, &t, &m)) {
	case WB_SOLVE_OK:
	case WB_SOLVE_CLAMPED:
		text += QStringLiteral("%1 %2     %3 %4")
				.arg(T("WhiteBalance.Temperature") + ":")
				.arg(t, 6, 'f', 1)
				.arg(T("WhiteBalance.Tint") + ":")
				.arg(m, 6, 'f', 1);
		break;
	case WB_SOLVE_TOO_DARK:
		text += T("WhiteBalance.Picker.TooDark");
		break;
	case WB_SOLVE_CLIPPED:
		text += T("WhiteBalance.Picker.Clipped");
		break;
	}
	readout_->setText(text);
}

void PickerDialog::onSampled(const QRect &r)
{
	if (frame_.isNull())
		return;
	double t = 0, m = 0;
	switch (wb_params_from_sample(averageLinear(r), &t, &m)) {
	case WB_SOLVE_OK:
		setStatus(QString(), false);
		break;
	case WB_SOLVE_CLAMPED:
		setStatus(T("WhiteBalance.Picker.ClampedWarn"), true);
		break;
	case WB_SOLVE_TOO_DARK:
		setStatus(T("WhiteBalance.Picker.TooDark"), true);
		return;
	case WB_SOLVE_CLIPPED:
		setStatus(T("WhiteBalance.Picker.Clipped"), true);
		return;
	}
	{
		QSignalBlocker b1(temp_), b2(tint_);
		temp_->setValue(t);
		tint_->setValue(m);
	}
	onSpinChanged();
}

void PickerDialog::onSpinChanged()
{
	applyToFilter(temp_->value(), tint_->value());
	refreshDisplay();
}

void PickerDialog::onSampleSizeChanged()
{
	const int n = sampleSize_->currentData().toInt();
	view_->setSampleSize(n);
	obs_data_t *s = obs_data_create();
	obs_data_set_int(s, S_SAMPLE_SIZE, n);
	obs_source_update(filter_, s);
	obs_data_release(s);
}

void PickerDialog::onReset()
{
	{
		QSignalBlocker b1(temp_), b2(tint_);
		temp_->setValue(0.0);
		tint_->setValue(0.0);
	}
	view_->setLastSample(QRect());
	setStatus(QString(), false);
	onSpinChanged();
}

void PickerDialog::applyToFilter(double temperature, double tint)
{
	obs_data_t *s = obs_data_create();
	obs_data_set_double(s, S_TEMPERATURE, temperature);
	obs_data_set_double(s, S_TINT, tint);
	obs_source_update(filter_, s);
	obs_data_release(s);
}

void PickerDialog::setStatus(const QString &text, bool warning)
{
	status_->setStyleSheet(warning ? QStringLiteral("color: #e8a33d;") : QString());
	status_->setText(text.isEmpty() ? QStringLiteral(" ") : text);
}

void PickerDialog::reject()
{
	obs_data_t *s = obs_data_create();
	obs_data_set_double(s, S_TEMPERATURE, origTemp_);
	obs_data_set_double(s, S_TINT, origTint_);
	obs_data_set_int(s, S_SAMPLE_SIZE, origSampleSize_);
	obs_source_update(filter_, s);
	obs_data_release(s);
	QDialog::reject();
}

/* ========================================================================= */

extern "C" bool wb_picker_run(obs_source_t *filter_source, struct wb_filter *f)
{
	if (!qApp || QThread::currentThread() != qApp->thread()) {
		obs_log(LOG_WARNING, "eyedropper can only be opened from the OBS user interface");
		return false;
	}

	QWidget *parent = QApplication::activeWindow();
	if (!parent)
		parent = static_cast<QWidget *>(obs_frontend_get_main_window());

	PickerDialog dlg(filter_source, f, parent);
	dlg.exec();
	return true;
}
