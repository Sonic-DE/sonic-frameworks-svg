/*
 *  SPDX-FileCopyrightText: 2026 Nicolas Fella <nicolas.fella@gmx.de>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QDirIterator>
#include <QSignalSpy>
#include <QTest>

#include <KColorScheme>
#include <KConfigGroup>

// Cursed way to access SvgPrivate
#define private public
#include "../src/ksvg/private/imageset_p.h"
#include "../src/ksvg/private/svg_p.h"
#include "imageset.h"
#include "svg.h"

using namespace Qt::Literals;

class SvgTest : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

private Q_SLOTS:
    void testSize();
    void testElements();
    void testColors();
    void testStylesheetOverrideColorChange();
    void testThemeColorsFollowAccentColor();
    void testDesktopThemeOpacity_data();
    void testDesktopThemeOpacity();
    void testDesktopThemeOpacityLive();
    void theSameImageIsHandedBackForTheSameRequest();

private:
    KSvg::Svg *m_svg;
    QDir m_themeDir;
    QDir m_cacheDir;
};

void copyDirectory(const QString &srcDir, const QString &dstDir)
{
    QDir targetDir(dstDir);
    QDirIterator it(srcDir, QDir::Filters(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Name), QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QString path = it.filePath();
        QString relDestPath = path.last(it.filePath().length() - srcDir.length() - 1);
        if (it.fileInfo().isDir()) {
            QVERIFY(targetDir.mkpath(relDestPath));
        } else {
            QVERIFY(QFile::copy(path, dstDir % '/' % relDestPath));
        }
    }
}

void SvgTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    m_themeDir = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) % '/' % "plasma");
    m_themeDir.removeRecursively();

    copyDirectory(QFINDTESTDATA("data/plasma"), m_themeDir.absolutePath());

    m_cacheDir = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    m_cacheDir.removeRecursively();

    m_svg = new KSvg::Svg;
    m_svg->setImagePath(QFINDTESTDATA("data/background.svgz"));
    QVERIFY(m_svg->isValid());
}

void SvgTest::cleanupTestCase()
{
    m_themeDir.removeRecursively();
}

void SvgTest::cleanup()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    KConfigGroup cg(config, "Colors:Window");
    cg.deleteEntry("BackgroundNormal");
}

void SvgTest::testSize()
{
    QSignalSpy spy(m_svg, &KSvg::Svg::sizeChanged);

    QCOMPARE(m_svg->size(), QSizeF(148, 148));

    m_svg->resize(500, 500);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_svg->size(), QSizeF(500, 500));

    m_svg->resize();
    QCOMPARE(spy.count(), 2);
    QCOMPARE(m_svg->size(), QSizeF(148, 148));
}

void SvgTest::theSameImageIsHandedBackForTheSameRequest()
{
    // The texture cache the QML items share keys on QImage::cacheKey(), so two items drawing one element
    // at one size hold a single texture between them only as long as the same image comes back for it.
    // Sixty list rows showing one arrow are sixty uploads and sixty atlas entries if this stops holding.
    const QSize size(25, 25);

    // Two items are two Svg objects, so the question is asked of two of them rather than of m_svg, which
    // the cases before this one leave carrying color overrides.
    KSvg::Svg one;
    one.setImagePath(QFINDTESTDATA("data/background.svgz"));
    QVERIFY(one.isValid());
    const QImage once = one.image(size, QString());
    QVERIFY(!once.isNull());
    QCOMPARE(one.image(size, QString()).cacheKey(), once.cacheKey());

    KSvg::Svg other;
    other.setImagePath(QFINDTESTDATA("data/background.svgz"));
    QVERIFY(other.isValid());
    QCOMPARE(other.image(size, QString()).cacheKey(), once.cacheKey());

    // A different size is a different picture, and has to be a different image.
    QVERIFY(one.image(QSize(26, 26), QString()).cacheKey() != once.cacheKey());

    // A color override is a different picture too, so it must not be served the same image.
    other.setColor(KSvg::Svg::StyleSheetColor::Text, Qt::magenta);
    QVERIFY(other.image(size, QString()).cacheKey() != once.cacheKey());
}

void SvgTest::testElements()
{
    QVERIFY(m_svg->hasElement("center"));
    QVERIFY(m_svg->hasElement("left"));
    QVERIFY(m_svg->hasElement("right"));
    QVERIFY(m_svg->hasElement("top"));
    QVERIFY(m_svg->hasElement("bottom"));
    QVERIFY(m_svg->hasElement("topleft"));
    QVERIFY(m_svg->hasElement("topright"));
    QVERIFY(m_svg->hasElement("bottomleft"));
    QVERIFY(m_svg->hasElement("bottomright"));

    QVERIFY(m_svg->hasElement("prefix-left"));
    QVERIFY(m_svg->hasElement("prefix-right"));
    QVERIFY(m_svg->hasElement("prefix-top"));
    QVERIFY(m_svg->hasElement("prefix-bottom"));
    QVERIFY(m_svg->hasElement("prefix-topleft"));
    QVERIFY(m_svg->hasElement("prefix-topright"));
    QVERIFY(m_svg->hasElement("prefix-bottomleft"));
    QVERIFY(m_svg->hasElement("prefix-bottomright"));

    QVERIFY(m_svg->hasElement("hint-left-margin"));
    QVERIFY(m_svg->hasElement("hint-right-margin"));
    QVERIFY(m_svg->hasElement("hint-top-margin"));
    QVERIFY(m_svg->hasElement("hint-bottom-margin"));

    QVERIFY(!m_svg->hasElement("banana"));

    QCOMPARE(m_svg->elementSize("left"), QSizeF(35, 26));

    QCOMPARE(m_svg->elementRect("left").toRect(), QRect(8, 71, 35, 26));
}

void SvgTest::testColors()
{
    KColorScheme windowColors(QPalette::Normal, KColorScheme::Window);
    KColorScheme selectionColors(QPalette::Active, KColorScheme::Selection);
    KColorScheme viewColors(QPalette::Active, KColorScheme::View);

    QCOMPARE(m_svg->color(KSvg::Svg::Text), windowColors.foreground(KColorScheme::NormalText));

    m_svg->setStatus(KSvg::Svg::Selected);
    QCOMPARE(m_svg->color(KSvg::Svg::Text), selectionColors.foreground(KColorScheme::NormalText));

    m_svg->setStatus(KSvg::Svg::Inactive);
    QCOMPARE(m_svg->color(KSvg::Svg::Text), selectionColors.foreground(KColorScheme::InactiveText));

    m_svg->setStatus(KSvg::Svg::Normal);
    m_svg->setColorSet(KSvg::Svg::View);
    QCOMPARE(m_svg->color(KSvg::Svg::Text), viewColors.foreground(KColorScheme::NormalText));

    m_svg->setStatus(KSvg::Svg::Selected);
    QCOMPARE(m_svg->color(KSvg::Svg::Text), selectionColors.foreground(KColorScheme::NormalText));

    m_svg->setStatus(KSvg::Svg::Normal);

    m_svg->setColor(KSvg::Svg::Text, QColor("#123456"));
    QCOMPARE(m_svg->color(KSvg::Svg::Text), QColor("#123456"));

    m_svg->clearColorOverrides();

    QCOMPARE(m_svg->color(KSvg::Svg::Text), viewColors.foreground(KColorScheme::NormalText));
}

void SvgTest::testStylesheetOverrideColorChange()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    KColorScheme windowColors(QPalette::Normal, KColorScheme::Window, config);
    auto set = new KSvg::ImageSet("testthemesystemcolors", "plasma/desktoptheme");
    m_svg->setImageSet(set);
    // Set a color override in the svg
    m_svg->setColor(KSvg::Svg::Text, Qt::blue);
    // set->setImageSetName(QStringLiteral("testthemesystemcolors"));

    QSignalSpy spy(set, &KSvg::ImageSet::imageSetChanged);

    QCOMPARE(m_svg->color(KSvg::Svg::Text), Qt::blue);

    // Generate pixmap and stylesheet
    m_svg->pixmap();
    // Since we set a color, stylesheetOverride must be present
    QVERIFY(!m_svg->d->stylesheetOverride.isEmpty());

    // Simulate an application color scheme change, setting background to red
    QPalette pal = qApp->palette();
    KConfigGroup cg(config, "Colors:Window");
    cg.writeEntry("BackgroundNormal", QStringLiteral("255,0,0"));
    KColorScheme::adjustForeground(pal, KColorScheme::NormalText, QPalette::WindowText, KColorScheme::Window, config);
    config->sync();
    qApp->setPalette(pal);
    // Wait the app color change is completely done, then regenerate pixmaps and stylesheets
    QVERIFY(spy.wait());
    m_svg->pixmap();
    // The background in stylesheetoverride is now red
    QVERIFY(m_svg->d->stylesheetOverride.contains(QStringLiteral(".ColorScheme-Background{color:#ff0000;}")));
}

void SvgTest::testThemeColorsFollowAccentColor()
{
    // A theme with its own colours and FollowAccentColor draws its highlight in the accent the
    // Colors settings wrote to kdeglobals, and in its own colour again without an accent.
    auto config = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    config->group(QStringLiteral("General")).writeEntry("AccentColor", QStringLiteral("61,212,37"));
    config->group(QStringLiteral("Colors:Selection")).writeEntry("BackgroundNormal", QStringLiteral("50,157,35"));
    config->sync();

    auto set = new KSvg::ImageSet(QStringLiteral("testthemeaccent"), QStringLiteral("plasma/desktoptheme"), this);
    KSvg::Svg svg;
    svg.setImageSet(set);
    svg.setImagePath(QStringLiteral("accent"));
    QVERIFY(svg.isValid());
    QCOMPARE(svg.color(KSvg::Svg::Highlight), QColor(50, 157, 35));
    QCOMPARE(svg.pixmap().toImage().pixelColor(5, 5), QColor(50, 157, 35));

    QSignalSpy spy(set, &KSvg::ImageSet::imageSetChanged);
    config->group(QStringLiteral("General")).deleteEntry("AccentColor");
    config->group(QStringLiteral("Colors:Selection")).deleteEntry("BackgroundNormal");
    config->sync();
    qApp->setPalette(QPalette(Qt::gray)); // what a colour scheme change sends
    QVERIFY(spy.wait());
    QCOMPARE(svg.color(KSvg::Svg::Highlight), QColor(1, 2, 3));
    QCOMPARE(svg.pixmap().toImage().pixelColor(5, 5), QColor(1, 2, 3));
}

namespace {
// Writes [DesktopThemeOpacity] (an invalid value removes the key) and tells the image set the way
// KConfigWatcher does; there is no session bus here. Waits until a changed setting is applied.
void notifyOpacity(KSvg::ImageSet* set, bool enabled, const QVariant& popups)
{
    auto config = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    KConfigGroup group = config->group(QStringLiteral("DesktopThemeOpacity"));
    group.writeEntry("Enabled", enabled);
    if (popups.isValid()) {
        group.writeEntry("Popups", popups.toInt());
    } else {
        group.deleteEntry("Popups");
    }
    config->sync();

    QSignalSpy spy(set, &KSvg::ImageSet::imageSetChanged);
    // One watcher is shared by every image set: earlier sets may still have a change pending too.
    Q_EMIT set->d->globalsWatcher->configChanged(group, {QByteArrayLiteral("Enabled"), QByteArrayLiteral("Popups")});
    if (set->d->updateNotificationTimer->isActive()) {
        QVERIFY(spy.wait());
    }
}

int popupAlpha(KSvg::Svg& svg)
{
    return qAlpha(svg.pixmap().toImage().pixel(5, 5));
}
}

void SvgTest::testDesktopThemeOpacity_data()
{
    QTest::addColumn<QString>("theme");
    QTest::addColumn<bool>("enabled");
    QTest::addColumn<QVariant>("popups");
    QTest::addColumn<int>("alpha");

    const QString follows = QStringLiteral("testthemeopacity");
    const QString own = QStringLiteral("testthemeopacityoff");
    QTest::newRow("switched off: the file's own opacity") << follows << false << QVariant(50) << 217;
    QTest::newRow("switched on without a value: the default") << follows << true << QVariant() << 217;
    QTest::newRow("switched on") << follows << true << QVariant(50) << 128;
    QTest::newRow("above 100 is opaque") << follows << true << QVariant(150) << 255;
    QTest::newRow("below 0 is clear") << follows << true << QVariant(-10) << 0;
    QTest::newRow("a theme that doesn't follow it") << own << true << QVariant(50) << 217;
}

void SvgTest::testDesktopThemeOpacity()
{
    QFETCH(QString, theme);
    QFETCH(bool, enabled);
    QFETCH(QVariant, popups);
    QFETCH(int, alpha);

    auto set = new KSvg::ImageSet(theme, QStringLiteral("plasma/desktoptheme"), this);
    notifyOpacity(set, enabled, popups);
    QVERIFY(!QTest::currentTestFailed());

    KSvg::Svg svg;
    svg.setImageSet(set);
    svg.setImagePath(QStringLiteral("popup"));
    QVERIFY(svg.isValid());
    QVERIFY2(qAbs(popupAlpha(svg) - alpha) <= 1, qPrintable(QString::number(popupAlpha(svg))));
}

void SvgTest::testDesktopThemeOpacityLive()
{
    // A change to the setting reaches images already drawn, without restarting.
    auto set = new KSvg::ImageSet(QStringLiteral("testthemeopacity"), QStringLiteral("plasma/desktoptheme"), this);
    notifyOpacity(set, true, 50);
    QVERIFY(!QTest::currentTestFailed());
    KSvg::Svg svg;
    svg.setImageSet(set);
    svg.setImagePath(QStringLiteral("popup"));
    QVERIFY2(qAbs(popupAlpha(svg) - 128) <= 1, qPrintable(QString::number(popupAlpha(svg))));

    notifyOpacity(set, true, 90);
    QVERIFY(!QTest::currentTestFailed());
    QVERIFY2(qAbs(popupAlpha(svg) - 230) <= 1, qPrintable(QString::number(popupAlpha(svg))));

    notifyOpacity(set, false, QVariant());
    QVERIFY2(qAbs(popupAlpha(svg) - 217) <= 1, qPrintable(QString::number(popupAlpha(svg))));
    auto config = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    config->deleteGroup(QStringLiteral("DesktopThemeOpacity"));
    config->sync();
}

QTEST_MAIN(SvgTest)

#include "svgtest.moc"
