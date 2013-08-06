/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2013 UniPro <ugene@unipro.ru>
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

#include <U2Core/DNAAlphabet.h>
#include <U2Core/DataBaseRegistry.h>
#include <U2Core/ExternalToolRegistry.h>
#include <U2Core/MultiTask.h>

#include <U2View/MSAEditor.h>
#include <U2View/MSAEditorFactory.h>
#include <U2View/DnaAssemblyUtils.h>
#include <U2Core/MAlignmentObject.h>

#include <U2Core/GAutoDeleteList.h>
#include <U2View/AnnotatedDNAView.h>
#include <U2View/ADVSequenceObjectContext.h>
#include <U2View/ADVConstants.h>
#include <U2View/ADVUtils.h>

#include <U2Core/DNASequenceSelection.h>
#include <U2Core/DNASequenceObject.h>
#include <U2Core/U2SafePoints.h>
#include <U2Core/ScriptingToolRegistry.h>
#include <U2Gui/GUIUtils.h>

#include <U2Test/XMLTestFormat.h>
#include <U2Test/GTest.h>
#include <U2Test/GTestFrameworkComponents.h>

#include <QtGui/QMenu>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>

#include "ExternalToolSupportPlugin.h"
#include "ExternalToolSupportSettings.h"
#include "ExternalToolSupportSettingsController.h"
#include "utils/ExternalToolSupportAction.h"
#include "utils/ExternalToolValidateTask.h"
#include "ETSProjectViewItemsContoller.h"

#include "clustalw/ClustalWSupport.h"
#include "clustalw/ClustalWWorker.h"
#include "clustalo/ClustalOSupport.h"
#include "clustalo/ClustalOWorker.h"
#include "mafft/MAFFTSupport.h"
#include "mafft/MAFFTWorker.h"
#include "tcoffee/TCoffeeSupport.h"
#include "tcoffee/TCoffeeWorker.h"
#include "mrbayes/MrBayesSupport.h"
#include "mrbayes/MrBayesTests.h"
#include "blast/FormatDBSupport.h"
#include "blast/BlastAllSupport.h"
#include "blast/BlastAllWorker.h"
#include "blast_plus/BlastPlusSupport.h"
#include "blast_plus/BlastPlusWorker.h"
#include "blast_plus/BlastDBCmdSupport.h"
#include "cap3/CAP3Support.h"
#include "bowtie/BowtieSupport.h"
#include "bowtie/BowtieTask.h"
#include "bowtie/BowtieSettingsWidget.h"
#include "bowtie/bowtie_tests/bowtieTests.h"
#include "bowtie2/Bowtie2Support.h"
#include "bwa/BwaSupport.h"
#include "bwa/BwaTask.h"
#include "bwa/BwaSettingsWidget.h"
#include "bwa/bwa_tests/bwaTests.h"
#include "samtools/SamToolsExtToolSupport.h"
#include "spidey/SpideySupport.h"
#include "spidey/SpideySupportTask.h"
#include "cufflinks/CuffdiffWorker.h"
#include "cufflinks/CufflinksSupport.h"
#include "cufflinks/CufflinksWorker.h"
#include "cufflinks/CuffmergeWorker.h"
#include "tophat/TopHatSupport.h"
#include "tophat/TopHatWorker.h"
#include "ceas/CEASReportWorker.h"
#include "ceas/CEASSupport.h"
#include "macs/MACSWorker.h"
#include "macs/MACSSupport.h"
#include "peak2gene/Peak2GeneWorker.h"
#include "peak2gene/Peak2GeneSupport.h"
#include "conservation_plot/ConservationPlotWorker.h"
#include "conservation_plot/ConservationPlotSupport.h"
#include "seqpos/SeqPosWorker.h"
#include "seqpos/SeqPosSupport.h"
#include "conduct_go/ConductGOWorker.h"
#include "conduct_go/ConductGOSupport.h"
#include "python/PythonSupport.h"
#include "perl/PerlSupport.h"
#include "R/RSupport.h"
#include "vcfutils/VcfutilsSupport.h"
#include "samtools/BcfToolsSupport.h"

#include <U2Algorithm/CDSearchTaskFactoryRegistry.h>
#include <U2Algorithm/DnaAssemblyAlgRegistry.h>
#include <blast_plus/RPSBlastSupportTask.h>


#define EXTERNAL_TOOL_SUPPORT_FACTORY_ID "ExternalToolSupport"
#define TOOLS "tools"

namespace U2 {

extern "C" Q_DECL_EXPORT Plugin * U2_PLUGIN_INIT_FUNC() {
    ExternalToolSupportPlugin * plug = new ExternalToolSupportPlugin();
    return plug;
}

/************************************************************************/
/* SearchToolsInPathTask */
/************************************************************************/
class SearchToolsInPathTask : public Task {
public:
    SearchToolsInPathTask(ExternalToolSupportPlugin *_plugin)
        : Task(tr("Search tools in PATH"), TaskFlag_NoRun), plugin(_plugin)
    {

    }

    void prepare() {
        QStringList envList = QProcess::systemEnvironment();
        if(envList.indexOf(QRegExp("PATH=.*",Qt::CaseInsensitive))>=0){
            QString pathEnv = envList.at(envList.indexOf(QRegExp("PATH=.*",Qt::CaseInsensitive)));
            QStringList paths;
#if defined(Q_OS_LINUX) || defined(Q_OS_MAC)
            paths = pathEnv.split("=").at(1).split(":");
#elif defined(Q_OS_WIN)
            paths = pathEnv.split("=").at(1).split(";");
#endif

            foreach(ExternalTool* curTool, AppContext::getExternalToolRegistry()->getAllEntries()){
                // UGENE-1781: Remove python external tool search in PATH
                // It should be fixed without crutches.
                if (curTool->getName() == PYTHON_TOOL_NAME) {
                    continue;
                }

                if(!curTool->getPath().isEmpty()) {
                    continue;
                }
                foreach(const QString& curPath, paths){
                    QString exePath = curPath+"/"+curTool->getExecutableFileName();
                    QFileInfo fileExe(exePath);
                    if(fileExe.exists() && (curTool->getPath()=="")){
                        ExternalToolValidateTask* validateTask=new ExternalToolValidateTask(curTool->getName(), exePath);
                        connect(validateTask, SIGNAL(si_stateChanged()), plugin, SLOT(sl_validateTaskStateChanged()));
                        addSubTask(validateTask);
                    }
                }
            }
        }
    }

private:
    ExternalToolSupportPlugin *plugin;
};

/************************************************************************/
/* ExternalToolSupportPlugin */
/************************************************************************/
ExternalToolSupportPlugin::ExternalToolSupportPlugin():Plugin(tr("External tool support"),tr("Runs other external tools")) {
    //External tools registry keeps order of items added
    //it is important because there might be dependencies

    //python
    PythonSupport* pythonSupport = new PythonSupport(PYTHON_TOOL_NAME);
    AppContext::getExternalToolRegistry()->registerEntry(pythonSupport);

    //perl
    PerlSupport *perlSupport = new PerlSupport(PERL_TOOL_NAME);
    AppContext::getExternalToolRegistry()->registerEntry(perlSupport);

    //Rscript
    RSupport* rSupport = new RSupport(R_TOOL_NAME);
    AppContext::getExternalToolRegistry()->registerEntry(rSupport);

    //Fill ExternalToolRegistry with supported tools
    //ClustalW
    ClustalWSupport* clustalWTool=new ClustalWSupport(CLUSTAL_TOOL_NAME);
    AppContext::getExternalToolRegistry()->registerEntry(clustalWTool);

    //ClustalO
    ClustalOSupport* clustalOTool=new ClustalOSupport(CLUSTALO_TOOL_NAME);
    AppContext::getExternalToolRegistry()->registerEntry(clustalOTool);

    //MAFFT
    MAFFTSupport* mAFFTTool=new MAFFTSupport(MAFFT_TOOL_NAME);
    AppContext::getExternalToolRegistry()->registerEntry(mAFFTTool);

    //T-Coffee
    TCoffeeSupport* tCoffeeTool=new TCoffeeSupport(TCOFFEE_TOOL_NAME);
    AppContext::getExternalToolRegistry()->registerEntry(tCoffeeTool);

    //MrBayes
    MrBayesSupport* mrBayesTool = new MrBayesSupport(MRBAYES_TOOL_NAME);
    AppContext::getExternalToolRegistry()->registerEntry(mrBayesTool);

    if (AppContext::getMainWindow()) {
        clustalWTool->getViewContext()->setParent(this);
        clustalWTool->getViewContext()->init();

        ExternalToolSupportAction* clustalWAction = new ExternalToolSupportAction(tr("ClustalW..."), this, QStringList(CLUSTAL_TOOL_NAME));
        clustalWAction->setObjectName("ClustalW");
        connect(clustalWAction, SIGNAL(triggered()), clustalWTool, SLOT(sl_runWithExtFileSpecify()));

        clustalOTool->getViewContext()->setParent(this);
        clustalOTool->getViewContext()->init();

        ExternalToolSupportAction* clustalOAction = new ExternalToolSupportAction(tr("ClustalO..."), this, QStringList(CLUSTALO_TOOL_NAME));
        connect(clustalOAction, SIGNAL(triggered()), clustalOTool, SLOT(sl_runWithExtFileSpecify()));

        mAFFTTool->getViewContext()->setParent(this);
        mAFFTTool->getViewContext()->init();

        ExternalToolSupportAction* mAFFTAction= new ExternalToolSupportAction(tr("MAFFT..."), this, QStringList(MAFFT_TOOL_NAME));
        mAFFTAction->setObjectName("MAFFT");
        connect(mAFFTAction, SIGNAL(triggered()), mAFFTTool, SLOT(sl_runWithExtFileSpecify()));

        tCoffeeTool->getViewContext()->setParent(this);
        tCoffeeTool->getViewContext()->init();

        ExternalToolSupportAction* tCoffeeAction= new ExternalToolSupportAction(tr("T-Coffee..."), this, QStringList(TCOFFEE_TOOL_NAME));
        tCoffeeAction->setObjectName("T-Coffee");
        connect(tCoffeeAction, SIGNAL(triggered()), tCoffeeTool, SLOT(sl_runWithExtFileSpecify()));

        //Add to menu MA
        QMenu* tools = AppContext::getMainWindow()->getTopLevelMenu(MWMENU_TOOLS);
        QMenu* toolsSubmenu = tools->findChild<QMenu*>(MWMENU_TOOLS_MALIGN);
        if(toolsSubmenu == NULL){
            toolsSubmenu = tools->addMenu(tr("Multiple alignment"));
            toolsSubmenu->setObjectName(MWMENU_TOOLS_MALIGN);
            toolsSubmenu->menuAction()->setObjectName("Multiple alignment");
        }

        toolsSubmenu->setIcon(QIcon(":core/images/msa.png"));//bad code
        toolsSubmenu->addAction(clustalWAction);
        toolsSubmenu->addAction(clustalOAction);
        toolsSubmenu->addAction(mAFFTAction);
        toolsSubmenu->addAction(tCoffeeAction);
    }
    ExternalToolRegistry *etRegistry = AppContext::getExternalToolRegistry();
    CHECK(NULL != etRegistry, );

    //FormatDB
    FormatDBSupport* formatDBTool = new FormatDBSupport(FORMATDB_TOOL_NAME);
    etRegistry->registerEntry(formatDBTool);

    //FormatDB from CUDA-BlastP
    FormatDBSupport* cudaFormatDBTool = new FormatDBSupport(CUDA_FORMATDB_TOOL_NAME);
    etRegistry->registerEntry(cudaFormatDBTool);

    //MakeBLASTDB from BLAST+
    FormatDBSupport* makeBLASTDBTool = new FormatDBSupport(MAKEBLASTDB_TOOL_NAME);
    etRegistry->registerEntry(makeBLASTDBTool);

    //MakeBLASTDB from GPU-BLAST+
//     FormatDBSupport* gpuMakeBLASTDBTool = new FormatDBSupport(GPU_MAKEBLASTDB_TOOL_NAME); // https://ugene.unipro.ru/tracker/browse/UGENE-945
//     etRegistry->registerEntry(gpuMakeBLASTDBTool);

    //BlastAll
    BlastAllSupport* blastallTool = new BlastAllSupport(BLASTALL_TOOL_NAME);
    etRegistry->registerEntry(blastallTool);

    //CUDA-BlastP
    BlastAllSupport* cudaBlastPTool = new BlastAllSupport(CUDA_BLASTP_TOOL_NAME);
    etRegistry->registerEntry(cudaBlastPTool);

    BlastPlusSupport* blastNPlusTool = new BlastPlusSupport(BLASTN_TOOL_NAME);
    etRegistry->registerEntry(blastNPlusTool);
    BlastPlusSupport* blastPPlusTool = new BlastPlusSupport(BLASTP_TOOL_NAME);
    etRegistry->registerEntry(blastPPlusTool);
//     BlastPlusSupport* gpuBlastPPlusTool = new BlastPlusSupport(GPU_BLASTP_TOOL_NAME); // https://ugene.unipro.ru/tracker/browse/UGENE-945
//     etRegistry->registerEntry(gpuBlastPPlusTool);
    BlastPlusSupport* blastXPlusTool = new BlastPlusSupport(BLASTX_TOOL_NAME);
    etRegistry->registerEntry(blastXPlusTool);
    BlastPlusSupport* tBlastNPlusTool = new BlastPlusSupport(TBLASTN_TOOL_NAME);
    etRegistry->registerEntry(tBlastNPlusTool);
    BlastPlusSupport* tBlastXPlusTool = new BlastPlusSupport(TBLASTX_TOOL_NAME);
    etRegistry->registerEntry(tBlastXPlusTool);
    BlastPlusSupport* rpsblastTool = new BlastPlusSupport(RPSBLAST_TOOL_NAME);
    etRegistry->registerEntry(rpsblastTool);
    BlastDbCmdSupport*  blastDbCmdSupport = new BlastDbCmdSupport();
    etRegistry->registerEntry(blastDbCmdSupport);

    // CAP3
    CAP3Support* cap3Tool = new CAP3Support(CAP3_TOOL_NAME);
    etRegistry->registerEntry(cap3Tool);

    // Bowtie
    BowtieSupport* bowtieSupport = new BowtieSupport(BOWTIE_TOOL_NAME);
    etRegistry->registerEntry(bowtieSupport);
    BowtieSupport* bowtieBuildSupport = new BowtieSupport(BOWTIE_BUILD_TOOL_NAME);
    etRegistry->registerEntry(bowtieBuildSupport);

    // Bowtie 2
    Bowtie2Support* bowtie2AlignSupport = new Bowtie2Support(BOWTIE2_ALIGN_TOOL_NAME);
    Bowtie2Support* bowtie2BuildSupport = new Bowtie2Support(BOWTIE2_BUILD_TOOL_NAME);
    Bowtie2Support* bowtie2InspectSupport = new Bowtie2Support(BOWTIE2_INSPECT_TOOL_NAME);
    AppContext::getExternalToolRegistry()->registerEntry(bowtie2AlignSupport);
    AppContext::getExternalToolRegistry()->registerEntry(bowtie2BuildSupport);
    AppContext::getExternalToolRegistry()->registerEntry(bowtie2InspectSupport);

    // BWA
    BwaSupport* bwaSupport = new BwaSupport(BWA_TOOL_NAME);
    etRegistry->registerEntry(bwaSupport);

    // SAMtools (external tool)
    SamToolsExtToolSupport* samToolsExtToolSupport = new SamToolsExtToolSupport(SAMTOOLS_EXT_TOOL_NAME);
    AppContext::getExternalToolRegistry()->registerEntry(samToolsExtToolSupport);

    // BCFtools (external tool)
    BcfToolsSupport* bcfToolsSupport = new BcfToolsSupport(BCFTOOLS_TOOL_NAME);
    AppContext::getExternalToolRegistry()->registerEntry(bcfToolsSupport);

    // Spidey
    SpideySupport* spideySupport = new SpideySupport(SPIDEY_TOOL_NAME);
    etRegistry->registerEntry(spideySupport);

    // TopHat
    TopHatSupport* tophatTool = new TopHatSupport(TOPHAT_TOOL_NAME);
    etRegistry->registerEntry(tophatTool);

    // Cufflinks external tools
    CufflinksSupport* cuffcompareTool = new CufflinksSupport(CUFFCOMPARE_TOOL_NAME);
    etRegistry->registerEntry(cuffcompareTool);
    CufflinksSupport* cuffdiffTool = new CufflinksSupport(CUFFDIFF_TOOL_NAME);
    etRegistry->registerEntry(cuffdiffTool);
    CufflinksSupport* cufflinksTool = new CufflinksSupport(CUFFLINKS_TOOL_NAME);
    etRegistry->registerEntry(cufflinksTool);
    CufflinksSupport* cuffmergeTool = new CufflinksSupport(CUFFMERGE_TOOL_NAME);
    etRegistry->registerEntry(cuffmergeTool);

    // CEAS
    CEASSupport *ceasTool = new CEASSupport(CEASSupport::TOOL_NAME);
    etRegistry->registerEntry(ceasTool);

    // MACS
    MACSSupport *macs = new MACSSupport(MACSSupport::TOOL_NAME);
    etRegistry->registerEntry(macs);

    // peak2gene
    Peak2GeneSupport *peak2gene = new Peak2GeneSupport(Peak2GeneSupport::TOOL_NAME);
    etRegistry->registerEntry(peak2gene);

    //ConservationPlot
    ConservationPlotSupport *conservationPlot = new ConservationPlotSupport(ConservationPlotSupport::TOOL_NAME);
    etRegistry->registerEntry(conservationPlot);

    //SeqPos
    SeqPosSupport *seqPos = new SeqPosSupport(SeqPosSupport::TOOL_NAME);
    etRegistry->registerEntry(seqPos);

    //ConductGO
    ConductGOSupport *conductGO = new ConductGOSupport(ConductGOSupport::TOOL_NAME);
    etRegistry->registerEntry(conductGO);

    //Vcfutils
    VcfutilsSupport *vcfutils = new VcfutilsSupport(VcfutilsSupport::TOOL_NAME);
    etRegistry->registerEntry(vcfutils);

    if (AppContext::getMainWindow()) {
        ExternalToolSupportAction* formatDBAction= new ExternalToolSupportAction(tr("FormatDB..."), this, QStringList(FORMATDB_TOOL_NAME));
        formatDBAction->setObjectName("FormatDB");
        connect(formatDBAction, SIGNAL(triggered()), formatDBTool, SLOT(sl_runWithExtFileSpecify()));

        ExternalToolSupportAction* makeBLASTDBAction= new ExternalToolSupportAction(tr("BLAST+ make DB..."), this, QStringList(MAKEBLASTDB_TOOL_NAME));
        makeBLASTDBAction->setObjectName("BLAST+ make DB");
        connect(makeBLASTDBAction, SIGNAL(triggered()), makeBLASTDBTool, SLOT(sl_runWithExtFileSpecify()));

        BlastAllSupportContext *blastAllViewContext = new BlastAllSupportContext(this);
        blastAllViewContext->setParent(this);
        blastAllViewContext->init();

        ExternalToolSupportAction* blastallAction= new ExternalToolSupportAction(tr("BLAST Search..."), this, QStringList(BLASTALL_TOOL_NAME));
        blastallAction->setObjectName("BLAST Search");
        connect(blastallAction, SIGNAL(triggered()), blastallTool, SLOT(sl_runWithExtFileSpecify()));
        

        BlastPlusSupportContext* blastPlusViewCtx = new BlastPlusSupportContext(this);
        blastPlusViewCtx->setParent(this);//may be problems???
        blastPlusViewCtx->init();
        QStringList toolList;
        toolList << BLASTN_TOOL_NAME << BLASTP_TOOL_NAME << BLASTX_TOOL_NAME << TBLASTN_TOOL_NAME << TBLASTX_TOOL_NAME << RPSBLAST_TOOL_NAME;
        ExternalToolSupportAction* blastPlusAction= new ExternalToolSupportAction(tr("BLAST+ Search..."), this, toolList);
        blastPlusAction->setObjectName("BLAST+ Search");
        connect(blastPlusAction, SIGNAL(triggered()), blastNPlusTool, SLOT(sl_runWithExtFileSpecify()));
        
        ExternalToolSupportAction* blastPlusCmdAction= new ExternalToolSupportAction(tr("BLAST+ query DB"), this, QStringList(BLASTDBCMD_TOOL_NAME));
        blastPlusCmdAction->setObjectName("BLAST+ query DB");
        connect(blastPlusCmdAction, SIGNAL(triggered()), blastDbCmdSupport, SLOT(sl_runWithExtFileSpecify()));



        //Add to menu NCBI Toolkit
        QMenu* tools = AppContext::getMainWindow()->getTopLevelMenu(MWMENU_TOOLS);
        QMenu* blastSubmenu = tools->addMenu(tr("BLAST"));
        blastSubmenu->setObjectName("BLAST");
        blastSubmenu->setIcon(QIcon(":external_tool_support/images/ncbi.png"));
        blastSubmenu->addAction(formatDBAction);
        blastSubmenu->addAction(blastallAction);
        blastSubmenu->addAction(makeBLASTDBAction);
        blastSubmenu->addAction(blastPlusAction);
        blastSubmenu->addAction(blastPlusCmdAction);
        
        ExternalToolSupportAction* cap3Action = new ExternalToolSupportAction(QString("Contig assembly with %1").arg(cap3Tool->getName()), this, QStringList(cap3Tool->getName()));
        cap3Action->setObjectName(QString("Contig assembly with %1").arg(cap3Tool->getName()));
        connect(cap3Action, SIGNAL(triggered()), cap3Tool, SLOT(sl_runWithExtFileSpecify()));
        QMenu* assemblySumbenu = tools->findChild<QMenu*>(MWMENU_TOOLS_ASSEMBLY);
        assemblySumbenu->addAction(cap3Action);

        GObjectViewWindowContext* spideyCtx = spideySupport->getViewContext();
        spideyCtx->setParent(this);
        spideyCtx->init();
    }

    AppContext::getCDSFactoryRegistry()->registerFactory(new CDSearchLocalTaskFactory(), CDSearchFactoryRegistry::LocalSearch);

    AppContext::getDnaAssemblyAlgRegistry()->registerAlgorithm(new DnaAssemblyAlgorithmEnv(BowtieTask::taskName, new BowtieTaskFactory(), 
        new BowtieGUIExtensionsFactory(), true/*Index*/, false /*Dbi*/, true/*Paired-reads*/));
    AppContext::getDnaAssemblyAlgRegistry()->registerAlgorithm(new DnaAssemblyAlgorithmEnv(BwaTask::ALGORITHM_BWA_ALN, new BwaTaskFactory(),
        new BwaGUIExtensionsFactory(), true/*Index*/, false/*Dbi*/, true/*Paired*/));
    AppContext::getDnaAssemblyAlgRegistry()->registerAlgorithm(new DnaAssemblyAlgorithmEnv(BwaTask::ALGORITHM_BWA_SW, new BwaTaskFactory(),
        new BwaSwGUIExtensionsFactory(), true/*Index*/, false/*Dbi*/, false/*Paired*/));

    {
        GTestFormatRegistry *tfr = AppContext::getTestFramework()->getTestFormatRegistry();
        XMLTestFormat *xmlTestFormat = qobject_cast<XMLTestFormat *>(tfr->findFormat("XML"));
        assert(NULL != xmlTestFormat);

        GAutoDeleteList<XMLTestFactory> *l = new GAutoDeleteList<XMLTestFactory>(this);
        l->qlist = BowtieTests::createTestFactories();

        foreach(XMLTestFactory *f, l->qlist) {
            bool res = xmlTestFormat->registerTestFactory(f);
            Q_UNUSED(res);
            assert(res);
        }
    }
    {
        GTestFormatRegistry *tfr = AppContext::getTestFramework()->getTestFormatRegistry();
        XMLTestFormat *xmlTestFormat = qobject_cast<XMLTestFormat *>(tfr->findFormat("XML"));
        assert(NULL != xmlTestFormat);

        GAutoDeleteList<XMLTestFactory> *l = new GAutoDeleteList<XMLTestFactory>(this);
        l->qlist = BwaTests::createTestFactories();

        foreach(XMLTestFactory *f, l->qlist) {
            bool res = xmlTestFormat->registerTestFactory(f);
            Q_UNUSED(res);
            assert(res);
        }
    }
    {

        GTestFormatRegistry* tfr = AppContext::getTestFramework()->getTestFormatRegistry();
        XMLTestFormat *xmlTestFormat = qobject_cast<XMLTestFormat*>(tfr->findFormat("XML"));
        assert(xmlTestFormat!=NULL);

        GAutoDeleteList<XMLTestFactory>* l = new GAutoDeleteList<XMLTestFactory>(this);
        l->qlist = MrBayesToolTests::createTestFactories();

        foreach(XMLTestFactory* f, l->qlist) { 
            bool res = xmlTestFormat->registerTestFactory(f);
            Q_UNUSED(res);
            assert(res);
        }
    }

    //Read settings
    ExternalToolSupportSettings::getExternalTools();
    
    //Search for tools in application dir
    
    QDir appDir(AppContext::getWorkingDirectoryPath());
    QStringList entryList = appDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QString toolsDir;
    foreach(const QString& dirName, entryList) {
        if (dirName.contains(TOOLS)) {
            toolsDir = appDir.absolutePath()+ "/" + dirName;
            break;
        }
    }    

    QList<Task*> tasks;
    if (!toolsDir.isEmpty()) {
        foreach(ExternalTool* curTool, etRegistry->getAllEntries()){
             if(!curTool->getPath().isEmpty()){ 
                 //check if runner exists otherwise run validation (for invalid external tools)

                 QString runner = curTool->getToolRunnerProgram();
                 if(runner.isEmpty()){
                    continue;
                 }

                 
                 ScriptingToolRegistry* str = AppContext::getScriptingToolRegistry();
                 if (str && str->getByName(runner) && curTool->isValid()){
                     continue;
                 }
             }
             QString exeName = curTool->getExecutableFileName();
             bool fileNotFound = true;
             LimitedDirIterator it (toolsDir);
             while (it.hasNext()&& fileNotFound) {
                 it.next();
                 QString toolPath(it.filePath() + "/" + exeName);
                 QFileInfo info(toolPath);
                 if(info.exists() && info.isFile()){
                     QString path = QDir::toNativeSeparators(toolPath);
                     ExternalToolValidateTask* validateTask=new ExternalToolValidateTask(curTool->getName(), path);
                     connect(validateTask,SIGNAL(si_stateChanged()),SLOT(sl_validateTaskStateChanged()));
                     tasks.append(validateTask);
                     fileNotFound=false;
                 }
             }
        }
    }
    if (!tasks.isEmpty()) {
        // Workaround: perl is found after the vcftool.
        // Should be fixed.
//        tasks << new SearchToolsInPathTask(this);
        tasks.prepend(new SearchToolsInPathTask(this));
        SequentialMultiTask* checkExternalToolsTask=new SequentialMultiTask(tr("Checking external tools for first time"), tasks, TaskFlags_NR_FOSCOE);
        AppContext::getTaskScheduler()->registerTopLevelTask(checkExternalToolsTask);
    } else {
        AppContext::getTaskScheduler()->registerTopLevelTask(new SearchToolsInPathTask(this));
    }

    if (AppContext::getMainWindow()) {
        //Add viewer for settings
        AppContext::getAppSettingsGUI()->registerPage(new ExternalToolSupportSettingsPageController());
    }
    //Add new workers to WD
    LocalWorkflow::ClustalWWorkerFactory::init();
    LocalWorkflow::ClustalOWorkerFactory::init();
    LocalWorkflow::MAFFTWorkerFactory::init();
    LocalWorkflow::BlastAllWorkerFactory::init();
    LocalWorkflow::BlastPlusWorkerFactory::init();
    LocalWorkflow::TCoffeeWorkerFactory::init();
    LocalWorkflow::CuffdiffWorkerFactory::init();
    LocalWorkflow::CufflinksWorkerFactory::init();
    LocalWorkflow::CuffmergeWorkerFactory::init();
    LocalWorkflow::TopHatWorkerFactory::init();
    LocalWorkflow::CEASReportWorkerFactory::init();
    LocalWorkflow::MACSWorkerFactory::init();
    LocalWorkflow::Peak2GeneWorkerFactory::init();
    LocalWorkflow::ConservationPlotWorkerFactory::init();
    LocalWorkflow::SeqPosWorkerFactory::init();
    LocalWorkflow::ConductGOWorkerFactory::init();

    if (AppContext::getMainWindow()) {
        //Add project view service
        services.push_back(new ExternalToolSupportService());
    }
}

ExternalToolSupportPlugin::~ExternalToolSupportPlugin(){
    ExternalToolSupportSettings::setExternalTools();
}
void ExternalToolSupportPlugin::sl_validateTaskStateChanged(){
    ExternalToolValidateTask* s=qobject_cast<ExternalToolValidateTask*>(sender());
    assert(s);
    if(s->isFinished()){
        AppContext::getExternalToolRegistry()->getByName(s->getToolName())->setValid(s->isValidTool());
        AppContext::getExternalToolRegistry()->getByName(s->getToolName())->setVersion(s->getToolVersion());
        AppContext::getExternalToolRegistry()->getByName(s->getToolName())->setPath(s->getToolPath());
        
    }
}
//////////////////////////////////////////////////////////////////////////
// Service
ExternalToolSupportService::ExternalToolSupportService()
: Service(Service_ExternalToolSupport, tr("External tools support"), tr("Provides support to run external tools from UGENE"), QList<ServiceType>() << Service_ProjectView)
{
    projectViewController = NULL;
}

void ExternalToolSupportService::serviceStateChangedCallback(ServiceState oldState, bool enabledStateChanged) {
    Q_UNUSED(oldState);

    if (!enabledStateChanged) {
        return;
    }
    if (isEnabled()) {
        projectViewController = new ETSProjectViewItemsContoller(this);
    } else {
        delete projectViewController; projectViewController = NULL;
    }
}

}
