/*
obs-whitebalance
Copyright (C) 2026 obs-whitebalance contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

/* Cosmetic tweaks to OBS's generic properties view, which offers no API for
 * button icons or field widths. OBS builds those widgets from our property
 * descriptions, so we recognise ours when they are first polished:
 *   - the "pick" button (by its label) becomes an eyedropper icon button
 *   - the sample-size combo (by its tooltip) stops stretching full width
 *   - the button is moved onto the sample-size row, right of the combo */

#include "wb-ui.hpp"
#include "wb-filter.h"

#include <obs-module.h>

#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPointer>
#include <QTimer>
#include <QPainterPath>
#include <QPushButton>

static QObject *g_tweaks = nullptr;

QIcon wb_eyedropper_icon(const QColor &color, int size)
{
	QIcon icon;
	for (qreal dpr : {1.0, 2.0}) {
		QPixmap pm(QSize(size, size) * dpr);
		pm.setDevicePixelRatio(dpr);
		pm.fill(Qt::transparent);

		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing, true);
		p.scale(size / 24.0, size / 24.0);
		p.translate(12, 12);
		p.rotate(45); /* bulb up-right, tip down-left */

		QPen pen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

		/* rubber bulb */
		p.setPen(Qt::NoPen);
		p.setBrush(color);
		p.drawRoundedRect(QRectF(-3.4, -11.6, 6.8, 7.2), 3.2, 3.2);
		/* collar */
		p.drawRoundedRect(QRectF(-5.0, -5.2, 10.0, 2.2), 0.8, 0.8);

		/* glass tube with tapered tip */
		QPainterPath tube;
		tube.moveTo(-2.0, -3.0);
		tube.lineTo(-2.0, 5.5);
		tube.lineTo(-0.5, 10.5);
		tube.lineTo(0.5, 10.5);
		tube.lineTo(2.0, 5.5);
		tube.lineTo(2.0, -3.0);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		p.drawPath(tube);
		p.end();

		icon.addPixmap(pm);
	}
	return icon;
}

namespace {

bool isSampleSizeCombo(QWidget *w)
{
	auto *combo = qobject_cast<QComboBox *>(w);
	return combo && combo->toolTip() == QString::fromUtf8(obs_module_text("WhiteBalance.SampleSize.Desc"));
}

/* OBS puts each property on its own QFormLayout row. Move the eyedropper
 * button next to the Sample Size combo and drop its (label-less) row. */
void mergeIntoSampleSizeRow(QPushButton *button)
{
	QWidget *container = button->parentWidget();
	auto *form = container ? qobject_cast<QFormLayout *>(container->layout()) : nullptr;
	if (!form)
		return;

	int buttonRow = -1;
	QFormLayout::ItemRole role;
	form->getWidgetPosition(button, &buttonRow, &role);
	if (buttonRow < 0)
		return;

	auto findComboRow = [form]() {
		for (int r = 0; r < form->rowCount(); r++) {
			QLayoutItem *it = form->itemAt(r, QFormLayout::FieldRole);
			if (it && isSampleSizeCombo(it->widget()))
				return r;
		}
		return -1;
	};
	if (findComboRow() < 0)
		return;

	/* take the button's row without deleting the button */
	QFormLayout::TakeRowResult taken = form->takeRow(buttonRow);
	delete taken.labelItem;
	delete taken.fieldItem;

	const int comboRow = findComboRow();
	QWidget *combo = form->itemAt(comboRow, QFormLayout::FieldRole)->widget();
	form->removeWidget(combo);

	auto *row = new QWidget(container);
	auto *h = new QHBoxLayout(row);
	h->setContentsMargins(0, 0, 0, 0);
	h->addWidget(combo);
	h->addWidget(button);
	h->addStretch(1);
	form->setWidget(comboRow, QFormLayout::FieldRole, row);
	combo->show();
	button->show();
}

class PropertiesTweaks : public QObject {
public:
	using QObject::QObject;

protected:
	bool eventFilter(QObject *obj, QEvent *ev) override
	{
		if (ev->type() != QEvent::Polish)
			return false;

		if (auto *button = qobject_cast<QPushButton *>(obj)) {
			const QString label = QString::fromUtf8(obs_module_text("WhiteBalance.Pick"));
			if (!button->text().isEmpty() && button->text() == label) {
				button->setText(QString());
				button->setToolTip(label);
				button->setAccessibleName(label);
				button->setIcon(wb_eyedropper_icon(button->palette().color(QPalette::ButtonText), 18));
				button->setIconSize(QSize(18, 18));
				button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
				button->setFixedWidth(44);
				/* defer until the whole properties view is built */
				QPointer<QPushButton> guard(button);
				QTimer::singleShot(0, button, [guard]() {
					if (guard)
						mergeIntoSampleSizeRow(guard);
				});
			}
		} else if (auto *combo = qobject_cast<QComboBox *>(obj)) {
			const QString tip = QString::fromUtf8(obs_module_text("WhiteBalance.SampleSize.Desc"));
			if (combo->toolTip() == tip) {
				combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
				combo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			}
		}
		return false;
	}
};

} // namespace

extern "C" void wb_ui_install(void)
{
	if (!qApp || g_tweaks)
		return;
	g_tweaks = new PropertiesTweaks(qApp);
	qApp->installEventFilter(g_tweaks);
}

extern "C" void wb_ui_uninstall(void)
{
	if (!g_tweaks)
		return;
	if (qApp)
		qApp->removeEventFilter(g_tweaks);
	delete g_tweaks;
	g_tweaks = nullptr;
}
