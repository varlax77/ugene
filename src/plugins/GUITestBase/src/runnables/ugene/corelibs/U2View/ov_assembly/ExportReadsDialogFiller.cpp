/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2014 UniPro <ugene@unipro.ru>
 * http://ugene.unipro.ru
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "ExportReadsDialogFiller.h"

#if (QT_VERSION < 0x050000) //Qt 5
#include <QtGui/QApplication>
#include <QtGui/QLineEdit>
#include <QtGui/QComboBox>
#include <QtGui/QCheckBox>
#else
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCheckBox>
#endif

#include "api/GTLineEdit.h"
#include "api/GTComboBox.h"
#include "api/GTCheckBox.h"
#include "api/GTWidget.h"

namespace U2 {

#define GT_CLASS_NAME "GTUtilsDialog::ExportReadsDialogFiller"

ExportReadsDialogFiller::ExportReadsDialogFiller(U2OpStatus &os, const QString &filePath, const QString format, bool addToProject)
    : Filler(os, "ExportReadsDialog"),
      filePath(filePath),
      format(format),
      addToProject(addToProject)
{
}

#define GT_METHOD_NAME "run"
void ExportReadsDialogFiller::run() {
    QWidget *dialog = QApplication::activeModalWidget();
    GT_CHECK(dialog != NULL, "dialog not found");

    QLineEdit* fileLineEdit = dialog->findChild<QLineEdit*>("filepathLineEdit");
    GT_CHECK(fileLineEdit != NULL, "File path lineEdit not found");
    GTLineEdit::setText(os, fileLineEdit, filePath);

    QComboBox *formatComboBox = dialog->findChild<QComboBox*>("documentFormatComboBox");
    GT_CHECK(formatComboBox != NULL, "Format comboBox not found");
    GTComboBox::setIndexWithText(os, formatComboBox, format);

    QCheckBox* addToPrj = dialog->findChild<QCheckBox*>("addToProjectCheckBox");
    GT_CHECK(addToPrj != NULL, "Add to project check box not found");
    GTCheckBox::setChecked(os, addToPrj, addToProject);

    QDialogButtonBox* box = dialog->findChild<QDialogButtonBox*>("buttonBox");
    GT_CHECK(box != NULL, "buttonBox is NULL");
    QPushButton* button = box->button(QDialogButtonBox::Ok);
    GT_CHECK(button !=NULL, "cancel button is NULL");
    GTWidget::click(os, button);
}

#undef GT_METHOD_NAME

#undef GT_CLASS_NAME

} // namespace