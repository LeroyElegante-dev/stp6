#include "ui/assets_tab.h"
#include "ui/theme.h"
#include "ui/dialogs.h"
#include <FL/fl_ask.H>
#include <algorithm>
#include <cstdio>

namespace {
    template<typename F> void runSafely(F&& f) {
        try { f(); }
        catch (std::exception& e) { fl_alert("Ошибка: %s", e.what()); }
    }
}

AssetsTab::AssetsTab(int X, int Y, int W, int H, Service& svc)
    : Fl_Group(X, Y, W, H), svc_(svc) {
    box(FL_FLAT_BOX);
    color(Theme::bg);

    filter_ = new Fl_Choice(0, 0, 140, Theme::BTN_H);
    filter_->add("Все|В стойке|На съёмке");
    filter_->value(0);
    filter_->callback(filterCb, this);

    searchBtn_  = Theme::secondary(0, 0, 100, Theme::BTN_H, "Поиск");
    historyBtn_ = Theme::secondary(0, 0, 100, Theme::BTN_H, "История");
    searchBtn_->callback(searchCb, this);
    historyBtn_->callback(historyCb, this);

    prevPageBtn_ = Theme::secondary(0, 0, 40, Theme::BTN_H, "◀");
    nextPageBtn_ = Theme::secondary(0, 0, 40, Theme::BTN_H, "▶");
    prevPageBtn_->callback(prevCb, this);
    nextPageBtn_->callback(nextCb, this);

    // pageLabel_ — без литерала в конструкторе, текст задаётся через copy_label
    pageLabel_ = new Fl_Box(0, 0, 80, Theme::BTN_H);
    pageLabel_->box(FL_NO_BOX);
    pageLabel_->labelcolor(Theme::textMuted);
    pageLabel_->labelfont(Theme::font);
    pageLabel_->labelsize(12);
    pageLabel_->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    pageLabel_->copy_label("Стр. 1");

    addBtn_    = Theme::primary  (0, 0, 120, Theme::BTN_H, "+ Техника");
    editBtn_   = Theme::secondary(0, 0, 120, Theme::BTN_H, "Изменить");
    copyBtn_   = Theme::secondary(0, 0, 120, Theme::BTN_H, "Копировать");
    deleteBtn_ = Theme::dangerBtn(0, 0, 100, Theme::BTN_H, "Удалить");
    addBtn_->callback(addCb, this);
    editBtn_->callback(editCb, this);
    copyBtn_->callback(copyCb, this);
    deleteBtn_->callback(deleteCb, this);

    issueBtn_     = Theme::primary  (0, 0, 110, Theme::BTN_H, "Выдать");
    bulkIssueBtn_ = Theme::secondary(0, 0, 160, Theme::BTN_H, "Выдать несколько");
    reassignBtn_  = Theme::secondary(0, 0, 120, Theme::BTN_H, "Сменить");
    returnBtn_    = Theme::secondary(0, 0, 110, Theme::BTN_H, "Принять");
    issueBtn_->callback(issueCb, this);
    bulkIssueBtn_->callback(bulkIssueCb, this);
    reassignBtn_->callback(reassignCb, this);
    returnBtn_->callback(returnCb, this);

    table_ = new DataTable(0, 0, 100, 100);
    table_->setup({"ID", "Инв.№", "Модель", "Категория", "Статус", "У кого"},
                  {1, 2, 5, 3, 2, 4});
    table_->setBadgeColumn(4, {
        {"В стойке",  Theme::success},
        {"На съёмке", Theme::accent}
    });
    table_->onActivate([this](long id){ doEdit(id); });
    table_->onSelect([this]{ updateButtons(); });

    end();
    layout();
    reload();
}

void AssetsTab::layout() {
    const int pad = Theme::PAD;
    const int gap = Theme::GAP;
    const int bh  = Theme::BTN_H;

    int cx = x() + pad;
    int cy = y() + pad;

    // Row 1: filter | search | history | ◀ N ▶
    filter_->resize(cx, cy, 140, bh); cx += 140 + gap;
    searchBtn_->resize(cx, cy, 100, bh); cx += 100 + gap;
    historyBtn_->resize(cx, cy, 100, bh); cx += 100 + gap;
    prevPageBtn_->resize(cx, cy, 40, bh); cx += 40 + 4;
    pageLabel_->resize(cx, cy, 80, bh); cx += 80 + 4;
    nextPageBtn_->resize(cx, cy, 40, bh);

    // Row 2: CRUD (right-aligned)
    cy += bh + 12;
    int rx = x() + w() - 2 * pad;
    deleteBtn_->resize(rx - deleteBtn_->w(), cy, deleteBtn_->w(), bh);
    copyBtn_->resize(deleteBtn_->x() - gap - copyBtn_->w(), cy, copyBtn_->w(), bh);
    editBtn_->resize(copyBtn_->x() - gap - editBtn_->w(), cy, editBtn_->w(), bh);
    addBtn_->resize(editBtn_->x() - gap - addBtn_->w(), cy, addBtn_->w(), bh);

    // Row 3: operations
    cy += bh + 12;
    cx = x() + pad;
    issueBtn_->resize(cx, cy, issueBtn_->w(), bh); cx += issueBtn_->w() + gap;
    bulkIssueBtn_->resize(cx, cy, bulkIssueBtn_->w(), bh); cx += bulkIssueBtn_->w() + gap;
    reassignBtn_->resize(cx, cy, reassignBtn_->w(), bh); cx += reassignBtn_->w() + gap;
    returnBtn_->resize(cx, cy, returnBtn_->w(), bh);

    int tableY = cy + bh + 16;
    int tableH = h() - (tableY - y()) - pad;
    if (tableH < 80) tableH = 80;
    table_->resize(x() + pad, tableY, w() - 2 * pad, tableH);
}

void AssetsTab::resize(int X, int Y, int W, int H) {
    Fl_Group::resize(X, Y, W, H);
    layout();
}

void AssetsTab::reload() {
    runSafely([&]{
        long keepId = table_->selectedId();
        std::string filter = filter_->text(filter_->value());

        auto paged = svc_.listAssetsPaged(currentPage_, PAGE_SIZE, filter);

        if (paged.items.empty() && currentPage_ > 0) {
            currentPage_ = 0;
            paged = svc_.listAssetsPaged(0, PAGE_SIZE, filter);
        }

        hasNext_ = paged.hasNext;
        current_ = paged.items;

        std::vector<std::vector<std::string>> data;
        std::vector<long> ids;
        for (const auto& r : current_) {
            bool assigned = r.asset.isAssigned();
            data.push_back({
                std::to_string(r.asset.id()),
                r.asset.inventoryNumber(),
                r.asset.model(),
                r.asset.category(),
                assigned ? "На съёмке" : "В стойке",
                r.holder.empty() ? "—" : r.holder
            });
            ids.push_back(r.asset.id());
        }
        table_->setRows(data, ids);
        if (keepId > 0) table_->selectById(keepId);
        updateButtons();
        updatePagination();
    });
}

void AssetsTab::updateButtons() {
    long id = table_->selectedId();
    auto it = std::find_if(current_.begin(), current_.end(),
        [id](const AssetRow& r){ return r.asset.id() == id; });

    if (it == current_.end()) {
        issueBtn_->deactivate();
        returnBtn_->deactivate();
        reassignBtn_->deactivate();
        return;
    }

    bool assigned = it->asset.isAssigned();
    if (assigned) {
        issueBtn_->deactivate();
        returnBtn_->activate();
        reassignBtn_->activate();
    } else {
        issueBtn_->activate();
        returnBtn_->deactivate();
        reassignBtn_->deactivate();
    }
}

void AssetsTab::updatePagination() {
    std::string text = "Стр. " + std::to_string(currentPage_ + 1);
    pageLabel_->copy_label(text.c_str());
    pageLabel_->redraw();

    if (currentPage_ == 0) prevPageBtn_->deactivate();
    else prevPageBtn_->activate();

    if (!hasNext_) nextPageBtn_->deactivate();
    else nextPageBtn_->activate();
}

void AssetsTab::doAdd() {
    auto* d = new AssetDialog([this](auto i, auto m, auto c){
        runSafely([&]{ svc_.addAsset(i, m, c); currentPage_ = 0; reload(); });
    });
    d->show(); while (d->shown()) Fl::wait(); delete d;
}

void AssetsTab::doEdit(long id) {
    runSafely([&]{
        for (const auto& r : current_) {
            if (r.asset.id() != id) continue;
            auto* d = new AssetDialog([this](auto, auto, auto){},
                r.asset.inventoryNumber(), r.asset.model(), r.asset.category());
            d->show(); while (d->shown()) Fl::wait(); delete d;
            return;
        }
    });
}

void AssetsTab::doCopy(long id) {
    runSafely([&]{
        for (const auto& r : current_) {
            if (r.asset.id() != id) continue;
            auto* d = new AssetDialog(
                [this, id](auto newInv, auto, auto){
                    runSafely([&]{
                        svc_.duplicateAsset(id, newInv);
                        currentPage_ = 0;
                        reload();
                    });
                },
                "", r.asset.model(), r.asset.category(), true);
            d->show(); while (d->shown()) Fl::wait(); delete d;
            return;
        }
    });
}

void AssetsTab::doDelete(long id) {
    if (fl_choice("Удалить технику и всю историю её выдач?",
                  "Отмена", "Удалить", nullptr) != 1) return;
    runSafely([&]{ svc_.deleteAssetCascade(id); reload(); });
}

void AssetsTab::doIssue(long id) {
    runSafely([&]{
        auto emps = svc_.activeEmployees();
        auto free = svc_.freeAssets();
        std::vector<std::pair<long, std::string>> e, a;
        for (const auto& emp : emps)
            e.push_back({emp.id(), emp.fullName()});
        for (const auto& ast : free)
            a.push_back({ast.id(), ast.inventoryNumber() + " — " + ast.model()});
        auto* d = new IssueDialog([this](long eid, long aid){
            runSafely([&]{ svc_.issue(aid, eid); reload(); });
        }, e, a);
        d->show(); while (d->shown()) Fl::wait(); delete d;
    });
}

void AssetsTab::doBulkIssue() {
    runSafely([&]{
        auto emps = svc_.activeEmployees();
        auto free = svc_.freeAssets();
        std::vector<std::pair<long, std::string>> e, a;
        for (const auto& emp : emps)
            e.push_back({emp.id(), emp.fullName()});
        for (const auto& ast : free)
            a.push_back({ast.id(),
                         ast.inventoryNumber() + " — " + ast.model() +
                         " (" + ast.category() + ")"});
        auto* d = new BulkIssueDialog(
            [this](const std::vector<long>& ids, long eid){
                runSafely([&]{ svc_.issueMultiple(ids, eid); reload(); });
            }, e, a);
        d->show(); while (d->shown()) Fl::wait(); delete d;
    });
}

void AssetsTab::doReassign(long id) {
    runSafely([&]{
        for (const auto& r : current_) {
            if (r.asset.id() != id) continue;
            if (!r.asset.isAssigned()) {
                fl_alert("Техника не выдана");
                return;
            }

            auto emps = svc_.activeEmployees();
            std::vector<std::pair<long, std::string>> e;
            for (const auto& emp : emps)
                e.push_back({emp.id(), emp.fullName()});

            std::string label = r.asset.inventoryNumber() + " — " +
                                r.asset.model();

            auto* d = new ReassignDialog(
                [this, id](long newEmpId){
                    runSafely([&]{ svc_.reassignAsset(id, newEmpId); reload(); });
                }, label, r.holder, e);
            d->show(); while (d->shown()) Fl::wait(); delete d;
            return;
        }
    });
}

void AssetsTab::doReturn(long id) {
    if (fl_choice("Принять технику обратно в стойку?",
                  "Отмена", "Принять", nullptr) != 1) return;
    runSafely([&]{ svc_.giveBack(id); reload(); });
}

void AssetsTab::doSearch() {
    auto* d = new SearchDialog(svc_);
    d->callback([](Fl_Widget* w, void*) { w->hide(); });
    d->show();
}

void AssetsTab::doHistory(long id) {
    runSafely([&]{
        for (const auto& r : current_) {
            if (r.asset.id() != id) continue;
            auto history = svc_.assetHistory(id);
            std::string label = r.asset.inventoryNumber() + " — " +
                                r.asset.model();
            auto* d = new HistoryDialog(label, history);
            d->callback([](Fl_Widget* w, void*) { w->hide(); delete w; });
            d->show();
            return;
        }
    });
}

void AssetsTab::filterCb(Fl_Widget*, void* v) {
    auto* t = (AssetsTab*)v;
    t->currentPage_ = 0;
    t->reload();
}

void AssetsTab::searchCb(Fl_Widget*, void* v) { ((AssetsTab*)v)->doSearch(); }

void AssetsTab::historyCb(Fl_Widget*, void* v) {
    auto* t = (AssetsTab*)v;
    long id = t->table_->selectedId();
    if (id < 0) { fl_alert("Выберите строку"); return; }
    t->doHistory(id);
}

void AssetsTab::prevCb(Fl_Widget*, void* v) {
    auto* t = (AssetsTab*)v;
    if (t->currentPage_ > 0) {
        t->currentPage_--;
        t->reload();
    }
}

void AssetsTab::nextCb(Fl_Widget*, void* v) {
    auto* t = (AssetsTab*)v;
    if (t->hasNext_) {
        t->currentPage_++;
        t->reload();
    }
}

void AssetsTab::addCb(Fl_Widget*, void* v) { ((AssetsTab*)v)->doAdd(); }

void AssetsTab::editCb(Fl_Widget*, void* v) {
    auto* t = (AssetsTab*)v;
    long id = t->table_->selectedId();
    if (id < 0) { fl_alert("Выберите строку"); return; }
    t->doEdit(id);
}

void AssetsTab::copyCb(Fl_Widget*, void* v) {
    auto* t = (AssetsTab*)v;
    long id = t->table_->selectedId();
    if (id < 0) { fl_alert("Выберите строку"); return; }
    t->doCopy(id);
}

void AssetsTab::deleteCb(Fl_Widget*, void* v) {
    auto* t = (AssetsTab*)v;
    long id = t->table_->selectedId();
    if (id < 0) { fl_alert("Выберите строку"); return; }
    t->doDelete(id);
}

void AssetsTab::issueCb(Fl_Widget*, void* v) {
    auto* t = (AssetsTab*)v;
    long id = t->table_->selectedId();
    if (id < 0) { fl_alert("Выберите технику"); return; }
    t->doIssue(id);
}

void AssetsTab::bulkIssueCb(Fl_Widget*, void* v) {
    ((AssetsTab*)v)->doBulkIssue();
}

void AssetsTab::reassignCb(Fl_Widget*, void* v) {
    auto* t = (AssetsTab*)v;
    long id = t->table_->selectedId();
    if (id < 0) { fl_alert("Выберите технику"); return; }
    t->doReassign(id);
}

void AssetsTab::returnCb(Fl_Widget*, void* v) {
    auto* t = (AssetsTab*)v;
    long id = t->table_->selectedId();
    if (id < 0) { fl_alert("Выберите технику"); return; }
    t->doReturn(id);
}