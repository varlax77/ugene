/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2016 UniPro <ugene@unipro.ru>
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

#include <U2Core/AppContext.h>
#include <U2Core/AppSettings.h>
#include <U2Core/AppResources.h>
#include <U2Core/Counter.h>

#include "HmmerSupport.h"
#include "HmmerBuildTask.h"

namespace U2 {

HmmerBuildSettings::HmmerBuildSettings()
    : modelConstructionStrategy(p7_ARCH_FAST),
      relativeSequenceWeightingStrategy(p7_WGT_PB),
      effectiveSequenceWeightingStrategy(p7_EFFN_ENTROPY),
      eset(-1.0),
      seed(42),
      symfrac(0.5),
      fragtresh(0.5),
      wid(0.62),
      ere(-1.0),
      esigma(45.0),
      eid(0.62),
      eml(200),
      emn(200),
      evl(200),
      evn(200),
      efl(100),
      efn(200),
      eft(0.04)
{

}

bool HmmerBuildSettings::validate() const {
    CHECK(0 <= symfrac && symfrac <= 1, false);
    CHECK(0 <= wid && wid <= 1, false);
    CHECK( 0 < eset, false);
    CHECK(-1 == ere || 0 < ere, false);
    CHECK(0 <= fragtresh && fragtresh <= 1, false);
    CHECK(0 < esigma, false);
    CHECK(0 <= eid && eid <= 1, false);
    CHECK(0 < eml, false);
    CHECK(0 < emn, false);
    CHECK(0 < evl, false);
    CHECK(0 < evn, false);
    CHECK(0 < efl, false);
    CHECK(0 < efn, false);
    CHECK(0 < wid && wid < 1, false);
    CHECK(0 < seed, false);

    return true;
}

HmmerBuildTask::HmmerBuildTask(const HmmerBuildSettings &settings, const QString &msaUrl)
    : ExternalToolRunTask(HmmerSupport::BUILD_TOOL, getArguments(settings, msaUrl), new ExternalToolLogParser()),
      settings(settings),
      stockholmMsaUrl(msaUrl)
{
    GCOUNTER(cvar, tvar, "UHMM3BuildTask");

    SAFE_POINT_EXT(settings.validate(), setError("Settings are invalid"), );

    setReportingSupported(true);
    setReportingEnabled(true);
}

const QString &HmmerBuildTask::getHmmProfileUrl() const {
    return settings.profileUrl;
}

QString HmmerBuildTask::getReport(const Task *task, const HmmerBuildSettings &settings, const QString &msaUrl) {
    QString res;

    res += "<table>";
    if (!msaUrl.isEmpty()) {
        res += "<tr><td><b>" + tr("Source alignment") + "</b></td><td>" + msaUrl + "</td></tr>";
    }
    res += "<tr><td><b>" + tr("Profile name") + "</b></td><td>" + settings.profileUrl + "</td></tr>";

    res += "<tr><td><b>" + tr("Options:") + "</b></td></tr>";
    res += "<tr><td><b>" + tr("Model construction strategies") + "</b></td><td>";
    switch (settings.modelConstructionStrategy) {
    case HmmerBuildSettings::p7_ARCH_FAST:
        res += "fast";
        break;
    case HmmerBuildSettings::p7_ARCH_HAND:
        res += "hand";
        break;
    default:
        assert(false);
    }
    res += "</td></tr>";

    res += "<tr><td><b>" + tr("Relative model construction strategies") + "</b></td><td>";
    switch (settings.relativeSequenceWeightingStrategy) {
    case HmmerBuildSettings::p7_WGT_GSC:
        res += tr("Gerstein/Sonnhammer/Chothia tree weights");
        break;
    case HmmerBuildSettings::p7_WGT_BLOSUM:
        res += tr("Henikoff simple filter weights" );
        break;
    case HmmerBuildSettings::p7_WGT_PB:
        res += tr("Henikoff position-based weights" );
        break;
    case HmmerBuildSettings::p7_WGT_NONE:
        res += tr("No relative weighting; set all to 1" );
        break;
    case HmmerBuildSettings::p7_WGT_GIVEN:
        res += tr("Weights given in MSA file" );
        break;
    default:
        assert(false);
    }
    res += "</td></tr>";

    res += "<tr><td><b>" + tr("Effective sequence weighting strategies") + "</b></td><td>";
    switch (settings.effectiveSequenceWeightingStrategy) {
    case HmmerBuildSettings::p7_EFFN_ENTROPY:
        res += tr("adjust effective sequence number to achieve relative entropy target");
        break;
    case HmmerBuildSettings::p7_EFFN_CLUST:
        res += tr("effective sequence number is number of single linkage clusters");
        break;
    case HmmerBuildSettings::p7_EFFN_NONE:
        res += tr("no effective sequence number weighting: just use number of sequences");
        break;
    case HmmerBuildSettings::p7_EFFN_SET:
        res += tr("set effective sequence number for all models to: %1" ).arg(settings.eset);
        break;
    default:
        assert(false);
    }
    res += "</td></tr>";

    if (task->hasError()) {
        res += "<tr><td><b>" + tr("Task finished with error: '%1'").arg(task->getError()) + "</b></td><td></td></tr>";
    }
    res += "</table>";

    return res;
}

QString HmmerBuildTask::generateReport() const {
    return getReport(this, settings, stockholmMsaUrl);
}

QStringList HmmerBuildTask::getArguments(const HmmerBuildSettings &settings, const QString &msaUrl) {
    QStringList arguments;

    switch (settings.modelConstructionStrategy) {
    case HmmerBuildSettings::p7_ARCH_FAST:
        arguments << "--fast";
        arguments << "--symfrac" << QString::number(settings.symfrac);
        break;
    case HmmerBuildSettings::p7_ARCH_HAND:
        arguments << "--hand";
        break;
    default:
        FAIL(tr("Unknown model construction strategy"), arguments);
    }

    switch (settings.relativeSequenceWeightingStrategy) {
    case HmmerBuildSettings::p7_WGT_NONE:
        arguments << "--wnone";
        break;
    case HmmerBuildSettings::p7_WGT_GIVEN:
        arguments << "--wgiven";
        break;
    case HmmerBuildSettings::p7_WGT_GSC:
        arguments << "--wgsc";
        break;
    case HmmerBuildSettings::p7_WGT_PB:
        arguments << "--wpb";
        break;
    case HmmerBuildSettings::p7_WGT_BLOSUM:
        arguments << "--wblosum";
        arguments << "--wid" << QString::number(settings.wid);
        break;
    default:
        FAIL(tr("Unknown relative sequence weighting strategy"), arguments);
    }

    switch (settings.effectiveSequenceWeightingStrategy) {
    case HmmerBuildSettings::p7_EFFN_NONE:
        arguments << "--enone";
        break;
    case HmmerBuildSettings::p7_EFFN_SET:
        arguments << "--eset" << QString::number(settings.eset);
        break;
    case HmmerBuildSettings::p7_EFFN_CLUST:
        arguments << "--eclust";
        arguments << "--eid" << QString::number(settings.eid);
        break;
    case HmmerBuildSettings::p7_EFFN_ENTROPY:
        arguments << "--eent";
        if (settings.ere > 0) {
            arguments << "--ere" << QString::number(settings.ere);
        }
        arguments << "--esigma" << QString::number(settings.esigma);
        break;
    default:
        FAIL(tr("Unknown effective sequence weighting strategy"), arguments);
    }

    arguments << "--cpu" << QString::number(AppContext::getAppSettings()->getAppResourcePool()->getIdealThreadCount());
    arguments << "--seed" << QString::number(settings.seed);
    arguments << "--fragthresh" << QString::number(settings.fragtresh);
    arguments << "--EmL" << QString::number(settings.eml);
    arguments << "--EmN" << QString::number(settings.emn);
    arguments << "--EvL" << QString::number(settings.evl);
    arguments << "--EvN" << QString::number(settings.evn);
    arguments << "--EfL" << QString::number(settings.efl);
    arguments << "--EfN" << QString::number(settings.efn);
    arguments << "--Eft" << QString::number(settings.eft);

    arguments << settings.profileUrl << msaUrl;

    return arguments;
}

}   // namespace U2
