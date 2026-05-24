#include "controllers.hpp"

#include <regex>
#include <sstream>

#include "utils.hpp"

namespace internstart {

PageRenderer::PageRenderer(std::string base_dir) : base_dir_(std::move(base_dir)) {}

namespace {

std::string cookieValue(const Request& request, const std::string& key) {
    auto it = request.headers.find("cookie");
    if (it == request.headers.end()) return "";
    std::string cookies = it->second;
    std::string needle = key + "=";
    size_t pos = cookies.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    size_t end = cookies.find(';', pos);
    return cookies.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
}

std::string activeClass(const std::string& active, const std::string& item) {
    return active == item ? "active" : "";
}

std::string bellIcon() {
    return "<svg class='bell-icon' viewBox='0 0 24 24' aria-hidden='true'><path d='M18 8a6 6 0 0 0-12 0c0 7-3 7-3 9h18c0-2-3-2-3-9'></path><path d='M13.73 21a2 2 0 0 1-3.46 0'></path></svg>";
}

std::string navIcon(const std::string& name) {
    if (name == "catalog") return "<svg viewBox='0 0 24 24' aria-hidden='true'><path d='M4 5h16'></path><path d='M4 12h16'></path><path d='M4 19h10'></path></svg>";
    if (name == "applications") return "<svg viewBox='0 0 24 24' aria-hidden='true'><rect x='4' y='4' width='16' height='16' rx='3'></rect><path d='M8 9h8'></path><path d='M8 14h5'></path></svg>";
    return "<svg viewBox='0 0 24 24' aria-hidden='true'><circle cx='12' cy='8' r='4'></circle><path d='M4 20c1.6-4 14.4-4 16 0'></path></svg>";
}

std::string accountMenu(const std::string& user_email) {
    std::ostringstream out;
    const std::string initial = user_email.empty() ? "А" : user_email.substr(0, 1);
    out << "<details class='account-menu nav-account-menu'><summary aria-label='Аккаунт'>" << htmlEscape(initial) << "</summary>"
        << "<div class='account-menu-panel'><span class='account-email'>" << htmlEscape(user_email) << "</span>"
        << "<div class='account-divider'></div>"
        << "<button class='account-notifications' type='button'>" << bellIcon() << "<span>Уведомления</span></button>"
        << "<div class='account-notification-panel is-hidden'><p class='muted'>Загрузка...</p></div>"
        << "<form method='post' action='/logout'><button class='btn btn-light btn-small'>Выйти</button></form></div></details>";
    return out.str();
}

std::string mobileTabBar(const std::string& active, const std::string& user_email) {
    std::ostringstream out;
    out << "<nav class='mobile-tabbar' aria-label='Основная навигация'>"
        << "<a class='" << activeClass(active, "catalog") << "' href='/internships'>" << navIcon("catalog") << "<span>Каталог</span></a>"
        << "<a class='" << activeClass(active, "applications") << "' href='/applications'>" << navIcon("applications") << "<span>Подачи</span></a>"
        << "<details class='mobile-account-menu account-menu'><summary aria-label='Аккаунт'>" << navIcon("account") << "<span>Аккаунт</span></summary>"
        << "<div class='account-menu-panel'><span class='account-email'>" << htmlEscape(user_email) << "</span>"
        << "<div class='account-divider'></div>"
        << "<button class='account-notifications' type='button'>" << bellIcon() << "<span>Уведомления</span></button>"
        << "<div class='account-notification-panel is-hidden'><p class='muted'>Загрузка...</p></div>"
        << "<form method='post' action='/logout'><button class='btn btn-light btn-small'>Выйти</button></form></div></details>"
        << "</nav>";
    return out.str();
}

std::string fixedOne(double value) {
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(1);
    out << value;
    return out.str();
}

std::string ratingStars(int rating, const std::string& class_name = "rating-stars") {
    std::ostringstream out;
    out << "<span class='" << class_name << "' aria-label='Оценка " << rating << " из 5'>";
    for (int i = 1; i <= 5; ++i) out << "<span class='" << (i <= rating ? "star star-on" : "star") << "'>★</span>";
    out << "</span>";
    return out.str();
}

std::string reviewCountLabel(int count) {
    int mod10 = count % 10;
    int mod100 = count % 100;
    if (mod10 == 1 && mod100 != 11) return std::to_string(count) + " отзыв";
    if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14)) return std::to_string(count) + " отзыва";
    return std::to_string(count) + " отзывов";
}

std::string textBlock(const std::string& value) {
    std::string text = value.empty() ? "Не указано" : value;
    text = std::regex_replace(text, std::regex("\\s+([2-9])\\.\\s+"), "\n$1. ");
    text = std::regex_replace(text, std::regex("\\s+([1-9][0-9])\\.\\s+"), "\n$1. ");
    std::ostringstream out;
    out << "<p class='text-block'>";
    for (char ch : htmlEscape(text)) {
        if (ch == '\n') out << "<br>";
        else out << ch;
    }
    out << "</p>";
    return out.str();
}

std::string directionOptions(const std::vector<Row>& rows, const std::string& selected, bool include_empty = false) {
    std::ostringstream out;
    if (include_empty) out << "<option value=''>Любое</option>";
    for (const auto& row : rows) {
        const std::string name = row.at("name");
        out << "<option value='" << htmlEscape(name) << "'" << (name == selected ? " selected" : "") << ">" << htmlEscape(name) << "</option>";
    }
    return out.str();
}

std::string companyCardStyle(const std::string& color) {
    std::string c = color.empty() ? "#0e7490" : color;
    if (c[0] != '#') c = "#" + c;
    if (c.size() != 7) c = "#0e7490";
    int r = std::stoi(c.substr(1, 2), nullptr, 16);
    int g = std::stoi(c.substr(3, 2), nullptr, 16);
    int b = std::stoi(c.substr(5, 2), nullptr, 16);
    std::ostringstream out;
    out << "--company-accent:" << c << ";"
        << "--company-soft-bg:rgba(" << r << "," << g << "," << b << ",0.12);"
        << "--company-border:rgba(" << r << "," << g << "," << b << ",0.34);"
        << "--company-hover:rgba(" << r << "," << g << "," << b << ",0.2);"
        << "--company-glow:rgba(" << r << "," << g << "," << b << ",0.28);";
    return out.str();
}

std::string displayDate(const std::string& value) {
    if (value.empty()) return "Не указана";
    if (value.size() == 10 && value[4] == '-' && value[7] == '-') {
        return value.substr(8, 2) + "." + value.substr(5, 2) + "." + value.substr(0, 4);
    }
    return value;
}

std::string displayTime(const std::string& value) {
    return value.empty() ? "Не указано" : value;
}

} // namespace

std::string PageRenderer::layout(const std::string& title, const std::string& body, bool admin, const std::string& active, const std::string& user_email) const {
    std::ostringstream out;
    out << "<!doctype html><html lang='ru'><head><meta charset='utf-8'>"
        << "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        << "<title>" << htmlEscape(title) << "</title>"
        << "<link rel='stylesheet' href='/static/styles.css'>"
        << "</head><body>";
    if (active != "landing") {
        out << "<header class='site-header " << (admin ? "admin-header" : "public-header") << "'><div class='container nav-wrap'>";
        if (active == "dashboard") {
            out << "<div class='account-header'><div class='account-brand'>InternStart</div>" << accountMenu(user_email) << "</div>";
        } else {
            out << "<nav class='top-switcher" << (admin ? " top-switcher-admin" : "") << "'>";
            if (!admin) {
                out << "<a class='" << activeClass(active, "catalog") << "' href='/internships'>Каталог</a>"
                    << "<a class='" << activeClass(active, "companies") << "' href='/companies'>Компании</a>"
                    << "<a class='" << activeClass(active, "applications") << "' href='/applications'>Мои подачи</a>"
                    << "<a class='" << activeClass(active, "ratings") << "' href='/ratings'>Мои оценки</a>"
                    << accountMenu(user_email);
            } else {
                out << "<a class='" << activeClass(active, "accounts") << "' href='/admin/accounts'>Аккаунты</a>"
                    << "<a class='" << activeClass(active, "admin") << "' href='/admin/internships'>Стажировки</a>"
                    << "<a class='" << activeClass(active, "companies") << "' href='/admin/companies'>Компании</a>"
                    << "<a class='" << activeClass(active, "directions") << "' href='/admin/directions'>Направления</a>"
                    << "<a class='" << activeClass(active, "imports") << "' href='/admin/imports'>Импорт</a>";
            }
        }
    }
    if (active != "landing") {
        if (active == "dashboard") out << "</div></header>";
        else out << "</nav></div></header>";
    }
    out << "<main class='container'>" << body << "</main>";
    if (!admin && active != "landing") {
        out << mobileTabBar(active, user_email);
        out << "<script>(()=>{const closePeers=(current)=>{document.querySelectorAll('details.account-menu[open]').forEach((item)=>{if(item!==current)item.removeAttribute('open');});};document.querySelectorAll('details.account-menu').forEach((menu)=>{menu.addEventListener('toggle',()=>{if(menu.open)closePeers(menu);});});document.querySelectorAll('.account-notifications').forEach((button)=>{button.addEventListener('click',async(event)=>{event.preventDefault();event.stopPropagation();const menu=button.closest('details.account-menu');const panel=menu.querySelector('.account-notification-panel');panel.classList.toggle('is-hidden');if(panel.classList.contains('is-hidden')||panel.dataset.loaded)return;try{const response=await fetch('/notifications');panel.innerHTML=await response.text();panel.dataset.loaded='1';}catch(e){panel.innerHTML='<p class=\"muted\">Не удалось загрузить уведомления.</p>';}});});})();</script>";
    }
    out << "</body></html>";
    return out.str();
}

Response PageRenderer::staticFile(const std::string& request_path) const {
    Response response;
    std::string body = readFile(base_dir_ + request_path);
    if (body.empty()) body = readFile(base_dir_ + "/cpp_backend" + request_path);
    if (body.empty()) body = readFile("." + request_path);
    if (body.empty()) body = readFile("./cpp_backend" + request_path);
    if (body.empty()) {
        response.status = 404;
        response.body = "Not found";
        return response;
    }
    if (request_path.size() >= 4 && request_path.substr(request_path.size() - 4) == ".png") {
        response.content_type = "image/png";
    } else {
        response.content_type = "text/css; charset=utf-8";
    }
    response.body = body;
    return response;
}

PublicController::PublicController(Database& db, const PageRenderer& renderer) : db_(db), renderer_(renderer) {}

Row PublicController::currentUser(const Request& request) const {
    std::string user_id = cookieValue(request, "user_id");
    if (user_id.empty()) return {};
    try {
        UserRepository users(db_);
        return users.findById(std::stoi(user_id));
    } catch (...) {
        return {};
    }
}

Response PublicController::requireUser(const Request& request) const {
    if (!currentUser(request).empty()) return Response{};
    return redirectTo("/");
}

Response PublicController::home(const Request& request) const {
    Row user = currentUser(request);
    std::ostringstream b;
    if (user.empty()) {
        b << "<div class='landing-page'>"
          << "<section class='home-visual-hero landing-hero'>"
          << "<img class='home-hero-image' src='/static/image.png' alt='InternStart workspace'>"
          << "<div class='home-hero-overlay'></div>"
          << "<div class='home-hero-content landing-content'>"
          << "<h1>Стажировки, компании и отклики в одном пространстве</h1>"
          << "<a class='btn btn-landing' href='#auth'>Начать карьерный путь</a>"
          << "</div></section>"
          << "<div class='auth-modal' id='auth'><a class='auth-backdrop' href='/' aria-label='Закрыть'></a>"
          << "<section class='auth-dialog'><a class='auth-close' href='/' aria-label='Закрыть'>×</a>"
          << "<h2>Вход в InternStart</h2>"
          << "<div class='auth-tabs'><input checked type='radio' name='auth_mode' id='auth-login'>"
          << "<input type='radio' name='auth_mode' id='auth-register'>"
          << "<div class='auth-tab-buttons'><label for='auth-login'>Вход</label><label for='auth-register'>Регистрация</label></div>";
        if (field(request.query_params, "auth") == "not_found") {
            b << "<p class='auth-message auth-message-warn'>Аккаунт с такой почтой не найден. Необходимо зарегистрироваться.</p>";
        } else if (field(request.query_params, "auth") == "wrong_password") {
            b << "<p class='auth-message auth-message-error'>Пароль неверный.</p>";
        }
        b
          << "<form method='post' action='/login' class='auth-form auth-login-form'>"
          << "<label>Почта<input name='email' type='email' placeholder='mail@example.com' autocomplete='email' required></label>"
          << "<label>Пароль<input name='password' type='password' placeholder='Ваш пароль' autocomplete='current-password' required></label>"
          << "<button class='btn btn-auth' type='submit'>Войти</button></form>"
          << "<form method='post' action='/register' class='auth-form auth-register-form'>"
          << "<label>Почта<input name='email' type='email' placeholder='mail@example.com' autocomplete='email' required></label>"
          << "<label>Пароль<input name='password' type='password' placeholder='Минимум 4 символа' autocomplete='new-password' required minlength='4'></label>"
          << "<button class='btn btn-auth' type='submit'>Создать аккаунт</button></form></div>"
          << "</section></div></div>";
        Response response;
        response.body = renderer_.layout("InternStart", b.str(), false, "landing");
        return response;
    }
    return redirectTo("/internships");
}

Response PublicController::registerUser(const Request& request) const {
    const std::string email = field(request.form, "email");
    const std::string password = field(request.form, "password");
    if (email.empty() || password.empty()) return redirectTo("/#auth");
    UserRepository users(db_);
    int id = users.create(email, password);
    Response response = redirectTo("/internships");
    response.headers.push_back("Set-Cookie: user_id=" + std::to_string(id) + "; Path=/; HttpOnly; SameSite=Lax");
    return response;
}

Response PublicController::loginUser(const Request& request) const {
    const std::string email = field(request.form, "email");
    const std::string password = field(request.form, "password");
    UserRepository users(db_);
    Row user = users.findByEmail(email);
    if (user.empty()) return redirectTo("/?auth=not_found#auth");
    if (user["password"] != password) return redirectTo("/?auth=wrong_password#auth");
    Response response = redirectTo("/internships");
    response.headers.push_back("Set-Cookie: user_id=" + user["id"] + "; Path=/; HttpOnly; SameSite=Lax");
    return response;
}

Response PublicController::logoutUser() const {
    Response response = redirectTo("/");
    response.headers.push_back("Set-Cookie: user_id=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax");
    return response;
}

Response PublicController::catalog(const Request& request) const {
    InternshipRepository internships(db_);
    DirectionRepository directions(db_);
    auto rows = internships.catalog(request.query_params);
    std::ostringstream b;
    b << "<h1>Каталог стажировок</h1>"
      << "<form method='get' class='filters command-panel'>"
      << "<div class='field search-row'><label>Поиск</label><input name='q' value='" << htmlEscape(field(request.query_params, "q")) << "' placeholder='Компания, направление, ключевые слова'></div>"
      << "<div class='field'><label>Город</label><input name='city' value='" << htmlEscape(field(request.query_params, "city")) << "' placeholder='Все'></div>"
      << "<div class='field'><label>Формат</label><select name='work_format'><option value=''>Любой</option><option value='office'>Офис</option><option value='remote'>Удалённо</option><option value='hybrid'>Гибрид</option></select></div>"
      << "<div class='field'><label>Направление</label><select name='direction'>" << directionOptions(directions.active(), field(request.query_params, "direction"), true) << "</select></div>"
      << "<div class='field'><label>Оплата</label><select name='paid'><option value=''>Любая</option><option value='1'>Оплачиваемая</option><option value='0'>Не оплачиваемая</option><option value='-1'>Не указано</option></select></div>"
      << "<div class='field'><label>Дата</label><select name='deadline'><option value=''>Любой</option><option value='has_deadline'>Есть дедлайн</option><option value='open_enrollment'>Набор открыт</option><option value='unknown'>Сроки не указаны</option></select></div>"
      << "<div class='field'><label>Сортировка</label><select name='sort'><option value='newest'>Сначала новые</option><option value='deadline'>Ближайший дедлайн</option><option value='company'>По компании</option></select></div>"
      << "<div class='actions'><button class='btn' type='submit'>Применить</button><a class='btn btn-light' href='/internships'>Сбросить</a></div></form>";
    if (rows.empty()) {
        b << "<p class='empty'>По вашему запросу ничего не найдено. Попробуйте изменить фильтры.</p>";
        Response response;
        Row user = currentUser(request);
        response.body = renderer_.layout("InternStart | Каталог", b.str(), false, "catalog", field(user, "email"));
        return response;
    }
    b << "<div class='cards-grid internships-grid'>";
    for (const auto& row : rows) {
        WorkFormat format = workFormatFromString(row.at("work_format"));
        std::string paid = "Оплата: не указано";
        if (row.at("is_paid") == "1") paid = "Оплачиваемая";
        if (row.at("is_paid") == "0") paid = "Не оплачиваемая";
        b << "<article class='card company-themed internship-card' style='" << companyCardStyle(row.count("company_accent_color") ? row.at("company_accent_color") : "#0e7490") << "'>"
          << "<div class='card-company-head'><p class='muted company-name-line'>";
        if (!row.at("company_slug").empty()) {
            b << "<a href='/companies/" << htmlEscape(row.at("company_slug")) << "'>" << htmlEscape(row.at("company_display_name")) << "</a>";
        } else {
            b << htmlEscape(row.at("company_display_name"));
        }
        b << " • " << htmlEscape(row.at("city")) << "</p></div>"
          << "<h3>" << htmlEscape(row.at("title")) << "</h3>"
          << "<p class='tags'><span>" << label(format) << "</span><span>" << htmlEscape(row.at("direction")) << "</span><span>" << paid << "</span></p>"
          << "<p class='muted'>Создано: " << htmlEscape(row.at("created_at").substr(0, 10)) << "</p>"
          << "<p class='muted'>Дата: " << htmlEscape(row.at("deadline_date").empty() ? "Не указана" : row.at("deadline_date")) << "</p>"
          << (!row.at("last_verified_at").empty() ? "<p class='verified-line'>Проверено: " + htmlEscape(row.at("last_verified_at").substr(0, 10)) + "</p>" : "")
          << "<p class='short-desc'>" << htmlEscape(row.at("short_description")) << "</p>"
          << "<div class='internship-accent-under'></div>"
          << "<a class='btn btn-light' href='/internships/" << row.at("id") << "'>Подробнее</a></article>";
    }
    b << "</div>";
    Response response;
    Row user = currentUser(request);
    response.body = renderer_.layout("InternStart | Каталог", b.str(), false, "catalog", field(user, "email"));
    return response;
}

Response PublicController::internshipDetail(int id, const Request& request) const {
    InternshipRepository internships(db_);
    ApplicationRepository applications(db_);
    CompanyReviewRepository reviews(db_);
    Row row = internships.publicDetail(id);
    if (row.empty()) {
        Response response;
        response.status = 404;
        Row user = currentUser(request);
        response.body = renderer_.layout("404", "<h1>Не найдено</h1>", false, "catalog", field(user, "email"));
        return response;
    }
    Row user = currentUser(request);
    Row attempt = applications.latestForInternship(id, user.empty() ? 0 : std::stoi(user["id"]));
    WorkFormat format = workFormatFromString(row["work_format"]);
    InternshipStatus status = internshipStatusFromString(row["status"]);
    CompanyRatingSummary summary;
    if (!row["company_id"].empty()) summary = reviews.summaryForCompany(std::stoi(row["company_id"]));
    std::ostringstream b;
    b << "<article class='detail'><h1>" << htmlEscape(row["title"]) << "</h1>";
    if (!row["company_slug"].empty()) {
        b << "<a class='company-inline company-inline-link company-themed' href='/companies/" << htmlEscape(row["company_slug"]) << "' style='" << companyCardStyle(row.count("company_accent_color") ? row["company_accent_color"] : "#0e7490") << "'>";
    } else {
        b << "<div class='company-inline company-themed' style='" << companyCardStyle(row.count("company_accent_color") ? row["company_accent_color"] : "#0e7490") << "'>";
    }
    b
      << "<p class='muted company-name-line'>";
    b << "<strong>" << htmlEscape(row["company_display_name"]) << "</strong>";
    b << " • " << htmlEscape(row["city"]) << "</p><span class='company-inline-rating'><strong>" << summary.averageText() << "</strong>"
      << ratingStars(summary.hasRatings() ? static_cast<int>(summary.average + 0.5) : 0, "rating-stars rating-stars-small")
      << "<span>" << reviewCountLabel(summary.count) << "</span></span>" << (row["company_slug"].empty() ? "</div>" : "</a>")
      << "<p class='tags'><span>" << label(format) << "</span><span>" << htmlEscape(row["direction"]) << "</span><span>" << label(status) << "</span></p>"
      << "<p class='muted'>Создано: " << htmlEscape(row["created_at"].substr(0, 10)) << "</p>"
      << "<div class='detail-grid'><div><strong>Тип:</strong> " << htmlEscape(row["employment_type"]) << "</div><div><strong>Оплата:</strong> " << (row["is_paid"] == "1" ? "Оплачиваемая" : row["is_paid"] == "0" ? "Не оплачиваемая" : "Не указано") << "</div>"
      << "<div><strong>Размер оплаты:</strong> " << htmlEscape(row["salary_info"].empty() ? "Не указано" : row["salary_info"]) << "</div><div><strong>Дата:</strong> " << htmlEscape(row["deadline_date"].empty() ? "Не указана" : row["deadline_date"]) << "</div></div>"
      << "<h2>Описание</h2>" << textBlock(row["full_description"])
      << "<h3>Требования</h3>" << textBlock(row["requirements"].empty() ? "Не указаны" : row["requirements"])
      << "<h3>Обязанности</h3>" << textBlock(row["responsibilities"].empty() ? "Не указаны" : row["responsibilities"])
      << "<h3>Условия</h3>" << textBlock(row["conditions"].empty() ? "Не указаны" : row["conditions"]) << "<div class='actions'>";
    if (attempt.empty()) {
        b << "<form method='post' action='/applications/add/" << id << "'><button class='btn'>Добавить в мои подачи</button></form>";
    } else {
        b << "<span class='badge badge-ok'>Последняя подача: " << label(applicationStatusFromString(attempt["status"])) << "</span><a class='btn btn-light' href='/applications'>Открыть доску</a>";
    }
    b << "<a class='btn' target='_blank' href='" << htmlEscape(row["source_url"]) << "'>Перейти к отклику</a></div></article>";
    Response response;
    response.body = renderer_.layout(row["title"], b.str(), false, "catalog", field(user, "email"));
    return response;
}

Response PublicController::companies(const Request& request) const {
    CompanyRepository companies(db_);
    auto rows = companies.activeCompaniesWithCounts();
    std::ostringstream b;
    b << "<section class='hero'><h1>Компании</h1><p>Исследуйте компании и смотрите все открытые стажировки по каждой из них.</p></section>";
    if (rows.empty()) b << "<p class='empty'>Пока нет активных компаний.</p>";
    else b << "<div class='cards-grid companies-grid'>";
    for (const auto& row : rows) {
        int rating_count = row.at("rating_count").empty() ? 0 : std::stoi(row.at("rating_count"));
        double rating_average = row.at("rating_average").empty() ? 0.0 : std::stod(row.at("rating_average"));
        b << "<article class='card company-themed company-card' style='" << companyCardStyle(row.at("accent_color")) << "'>"
          << "<div class='card-accent-line'></div><div class='card-company-head'><h3>" << htmlEscape(row.at("name")) << "</h3></div>"
          << "<p class='muted'>" << htmlEscape(row.at("description").empty() ? "Описание компании скоро появится." : row.at("description")) << "</p>"
          << "<div class='company-rating-mini'><strong>" << (rating_count ? fixedOne(rating_average) : "—") << "</strong>"
          << ratingStars(rating_count ? static_cast<int>(rating_average + 0.5) : 0, "rating-stars rating-stars-small")
          << "<span>" << reviewCountLabel(rating_count) << "</span></div>"
          << "<p class='tags'><span>" << row.at("active_internships_count") << " активных стажировок</span>"
          << (!row.at("career_url").empty() ? "<span>Есть карьерная страница</span>" : "") << "</p>"
          << "<div class='internship-accent-under'></div><a class='btn btn-light' href='/companies/" << htmlEscape(row.at("slug")) << "'>Подробнее</a></article>";
    }
    if (!rows.empty()) b << "</div>";
    Response response;
    Row user = currentUser(request);
    response.body = renderer_.layout("InternStart | Компании", b.str(), false, "companies", field(user, "email"));
    return response;
}

Response PublicController::companyDetail(const std::string& slug, const Request& request) const {
    CompanyRepository companies(db_);
    InternshipRepository internships(db_);
    CompanyReviewRepository reviews(db_);
    Row company = companies.findActiveBySlug(slug);
    if (company.empty()) {
        Response response;
        response.status = 404;
        Row user = currentUser(request);
        response.body = renderer_.layout("404", "<h1>Компания не найдена</h1>", false, "companies", field(user, "email"));
        return response;
    }
    auto rows = internships.byCompany(std::stoi(company["id"]));
    Row user = currentUser(request);
    int company_id = std::stoi(company["id"]);
    int user_id = user.empty() ? 0 : std::stoi(user["id"]);
    CompanyRatingSummary summary = reviews.summaryForCompany(company_id);
    auto review_rows = reviews.reviewsForCompany(company_id);
    auto eligible_attempts = user_id ? reviews.eligibleAttempts(user_id, company_id) : std::vector<Row>{};
    const bool can_review = !eligible_attempts.empty();
    std::ostringstream b;
    b << "<article class='detail company-themed' style='" << companyCardStyle(company["accent_color"]) << "'>"
      << "<div class='company-detail-top'><div><div class='card-company-head'><h1>" << htmlEscape(company["name"]) << "</h1></div>"
      << "<p>" << htmlEscape(company["description"]) << "</p></div>"
      << "<aside class='rating-summary'><div class='rating-number'>" << summary.averageText() << "</div>"
      << ratingStars(summary.hasRatings() ? static_cast<int>(summary.average + 0.5) : 0)
      << "<p class='muted'>" << reviewCountLabel(summary.count) << "</p>"
      << "<a class='btn btn-light rating-summary-link' href='#reviews-modal'>Посмотреть отзывы</a></aside></div><div class='actions'>";
    if (!company["website_url"].empty()) b << "<a class='btn btn-light' target='_blank' rel='noopener noreferrer' href='" << htmlEscape(company["website_url"]) << "'>Сайт компании</a>";
    if (!company["career_url"].empty()) b << "<a class='btn btn-light' target='_blank' rel='noopener noreferrer' href='" << htmlEscape(company["career_url"]) << "'>Карьера и стажировки</a>";
    if (can_review) {
        b << "<a class='btn' href='/ratings/companies/" << company_id << "'>Оставить отзыв</a>";
    }
    b << "</div></article>";

    b << "<div class='reviews-modal' id='reviews-modal'><a class='reviews-modal-backdrop' href='#' aria-label='Закрыть'></a>"
      << "<section class='reviews-dialog'><div class='edit-dialog-head'><div><h2>Отзывы</h2><p class='muted'>" << reviewCountLabel(summary.count) << ", оценки анонимные</p></div><a class='edit-close' href='#' aria-label='Закрыть'>×</a></div>";
    if (review_rows.empty()) {
        b << "<p class='empty'>Пока нет отзывов от стажеров с оффером или отказом.</p>";
    } else {
        b << "<div class='reviews-list'>";
        for (const auto& review : review_rows) {
            int rating = std::stoi(review.at("rating"));
            b << "<article class='review-card'><div class='review-head'><div>" << ratingStars(rating, "rating-stars rating-stars-small")
              << "<strong>" << rating << "/5</strong></div><span class='muted'>Анонимно</span></div>"
              << "<p class='review-vacancy'>" << htmlEscape(review.at("internship_title")) << " · " << label(applicationStatusFromString(review.at("application_status"))) << "</p>";
            if (!review.at("comment").empty()) b << "<p class='review-comment'>" << htmlEscape(review.at("comment")) << "</p>";
            b << "</article>";
        }
        b << "</div>";
    }
    b << "</section></div><section><h2>Опубликованные стажировки</h2>";
    if (rows.empty()) b << "<p class='empty'>У этой компании пока нет опубликованных стажировок.</p>";
    else b << "<div class='cards-grid internships-grid'>";
    for (const auto& row : rows) {
        b << "<article class='card company-themed internship-card' style='" << companyCardStyle(company["accent_color"]) << "'>"
          << "<h3>" << htmlEscape(row.at("title")) << "</h3><p class='muted'>" << htmlEscape(row.at("city")) << "</p>"
          << "<p class='tags'><span>" << label(workFormatFromString(row.at("work_format"))) << "</span><span>" << htmlEscape(row.at("direction")) << "</span></p>"
          << "<p class='short-desc'>" << htmlEscape(row.at("short_description")) << "</p><div class='internship-accent-under'></div>"
          << "<a class='btn btn-light' href='/internships/" << row.at("id") << "'>Открыть стажировку</a></article>";
    }
    if (!rows.empty()) b << "</div>";
    b << "</section>";
    Response response;
    response.body = renderer_.layout(company["name"], b.str(), false, "companies", field(user, "email"));
    return response;
}

Response PublicController::notifications(const Request& request) const {
    Row user = currentUser(request);
    Response response;
    if (user.empty()) {
        response.status = 403;
        response.body = "<p class='muted'>Нужно войти.</p>";
        return response;
    }
    const std::string today = nowIso().substr(0, 10);
    // Задачи с датой выполнения
    auto task_rows = db_.query(
        "SELECT a.*, i.title AS internship_title, COALESCE(c.name, i.company_name) AS company_display_name, "
        "(SELECT h.event_note FROM application_history h WHERE h.attempt_id=a.id AND h.event_type!='completion' ORDER BY h.id DESC LIMIT 1) AS task_note "
        "FROM application_attempts a JOIN internships i ON i.id=a.internship_id LEFT JOIN companies c ON c.id=i.company_id "
        "WHERE a.user_id=? AND a.marker_enabled=1 AND a.stage_completed=0 AND a.next_step_date IS NOT NULL AND a.next_step_date!='' "
        "ORDER BY a.next_step_date, a.next_step_time, i.title LIMIT 10",
        {user["id"]});
    std::ostringstream b;
    b << "<div class='notification-head'><strong>Уведомления</strong><span>" << today << "</span></div>";
    b << "<section class='notification-section'><h3>Список задач</h3>";
    if (task_rows.empty()) {
        b << "<p class='notification-empty'>Задач с датой выполнения нет.</p>";
    } else {
        b << "<div class='notification-list'>";
        for (const auto& row : task_rows) {
            bool overdue = row.at("next_step_date") < today;
            b << "<a class='notification-item" << (overdue ? " notification-item-overdue" : "") << "' href='/applications/" << row.at("id") << "/edit'><strong>"
              << (overdue ? "Просрочено: " : "") << htmlEscape(displayDate(row.at("next_step_date")))
              << (!row.at("next_step_time").empty() ? " " + htmlEscape(row.at("next_step_time")) : "") << "</strong><span>"
              << htmlEscape(row.at("internship_title")) << " · " << htmlEscape(row.at("company_display_name")) << "</span>";
            if (!row.at("task_note").empty()) b << "<em>" << htmlEscape(row.at("task_note")) << "</em>";
            b << "</a>";
        }
        b << "</div>";
    }
    b << "</section>";
    response.body = b.str();
    return response;
}

Response PublicController::myRatings(const Request& request) const {
    Row user = currentUser(request);
    CompanyReviewRepository reviews(db_);
    auto rows = reviews.eligibleCompanies(std::stoi(user["id"]));
    std::ostringstream b;
    b << "<div class='page-head'><div><h1>Мои оценки</h1><p class='muted'>Оценивать можно только компании, где подача дошла до оффера или отказа.</p></div><a class='btn btn-light' href='/applications'>К подачам</a></div>";
    if (rows.empty()) {
        b << "<p class='empty'>Пока нет компаний для оценки. Перенесите подачу в колонку «Оффер» или «Отказ».</p>";
    } else {
        b << "<div class='cards-grid companies-grid'>";
        for (const auto& row : rows) {
            const bool reviewed = row.at("reviewed") == "1";
            int user_rating = row.at("user_rating").empty() ? 0 : std::stoi(row.at("user_rating"));
            b << "<article class='card company-themed company-card rating-company-card' style='" << companyCardStyle(row.at("accent_color")) << "'>"
              << "<div class='card-company-head'><h3>" << htmlEscape(row.at("name")) << "</h3></div>"
              << "<p class='muted'>" << row.at("attempts_count") << " завершенных подач доступны для отзыва</p>"
              << "<div class='company-rating-mini'><strong>" << (reviewed ? std::to_string(user_rating) + "/5" : "Ждет") << "</strong>"
              << ratingStars(user_rating, "rating-stars rating-stars-small") << "<span>" << (reviewed ? "Ваша оценка сохранена" : "Можно оценить") << "</span></div>"
              << "<div class='internship-accent-under'></div><a class='btn btn-light' href='/ratings/companies/" << row.at("id") << "'>" << (reviewed ? "Изменить оценку" : "Оценить") << "</a></article>";
        }
        b << "</div>";
    }
    Response response;
    response.body = renderer_.layout("Мои оценки | InternStart", b.str(), false, "ratings", field(user, "email"));
    return response;
}

Response PublicController::rateCompany(int company_id, const Request& request) const {
    Row user = currentUser(request);
    CompanyRepository companies(db_);
    CompanyReviewRepository reviews(db_);
    Row company = companies.findById(company_id);
    if (company.empty()) {
        Response response;
        response.status = 404;
        response.body = renderer_.layout("404", "<h1>Компания не найдена</h1>", false, "ratings", field(user, "email"));
        return response;
    }
    auto attempts = reviews.eligibleAttempts(std::stoi(user["id"]), company_id);
    if (attempts.empty()) return redirectTo("/ratings");
    if (request.method == "POST") {
        reviews.saveReview(std::stoi(user["id"]), company_id, request.form);
        return redirectTo("/companies/" + company["slug"]);
    }
    Row existing = reviews.userReview(std::stoi(user["id"]), company_id);
    std::string selected_attempt = field(existing, "attempt_id", attempts.front().at("id"));
    std::string selected_rating = field(existing, "rating", "5");
    std::ostringstream b;
    b << "<div class='page-head'><div><h1>Оценка компании</h1><p class='muted'>" << htmlEscape(company["name"]) << "</p></div><a class='btn btn-light' href='/ratings'>Мои оценки</a></div>"
      << "<form class='review-form detail company-themed' style='" << companyCardStyle(company["accent_color"]) << "' method='post' action='/ratings/companies/" << company_id << "'>"
      << "<label>Стажировка<select name='attempt_id'>";
    for (const auto& attempt : attempts) {
        b << "<option value='" << attempt.at("id") << "'" << (attempt.at("id") == selected_attempt ? " selected" : "") << ">"
          << htmlEscape(attempt.at("internship_title")) << " · " << label(applicationStatusFromString(attempt.at("status"))) << "</option>";
    }
    b << "</select></label><fieldset class='rating-choice'><legend>Оценка</legend>";
    for (int i = 5; i >= 1; --i) {
        b << "<label><input type='radio' name='rating' value='" << i << "'" << (selected_rating == std::to_string(i) ? " checked" : "") << "><span>" << i << "</span></label>";
    }
    b << "</fieldset><label>Анонимный отзыв<textarea name='comment' rows='6' maxlength='1200' placeholder='Что было хорошо, что стоит знать будущим стажерам'>" << htmlEscape(field(existing, "comment")) << "</textarea></label>"
      << "<button class='btn' type='submit'>Сохранить оценку</button></form>";
    Response response;
    response.body = renderer_.layout("Оценка компании | InternStart", b.str(), false, "ratings", field(user, "email"));
    return response;
}

ApplicationController::ApplicationController(Database& db, const PageRenderer& renderer, int user_id) : db_(db), renderer_(renderer), user_id_(user_id) {}

Response ApplicationController::newInternship(const Request& request) const {
    if (request.method == "POST") {
        CompanyRepository companies(db_);
        DirectionRepository directions(db_);
        Row company;
        const std::string company_id = field(request.form, "company_id");
        if (!company_id.empty() && company_id != "custom") {
            company = companies.findById(std::stoi(company_id));
        }
        if (company.empty()) company = companies.ensurePrivateByName(field(request.form, "company_name", "Без компании"), user_id_);
        std::string direction = field(request.form, "direction", "other");
        if (direction == "custom") direction = field(request.form, "custom_direction", "other");
        directions.ensure(direction);
        const std::string description = field(request.form, "description");
        Row form = {
            {"title", field(request.form, "title", "Своя стажировка")},
            {"company_id", company["id"]},
            {"city", field(request.form, "city", "Не указано")},
            {"work_format", field(request.form, "work_format", "remote")},
            {"direction", direction},
            {"employment_type", field(request.form, "employment_type", "Стажировка")},
            {"short_description", description},
            {"full_description", description},
            {"source_url", field(request.form, "source_url")},
            {"status", "open"},
            {"is_published", "1"},
            {"created_by_type", "user"},
            {"created_by_user_id", std::to_string(user_id_)}
        };
        InternshipRepository internships(db_);
        int internship_id = internships.save(form);
        if (internship_id > 0) {
            ApplicationRepository applications(db_);
            applications.createAttempt(internship_id, user_id_, ApplicationStatus::WantToApply);
        }
        return redirectTo("/applications");
    }

    CompanyRepository companies(db_);
    DirectionRepository directions(db_);
    auto company_rows = companies.activeForSelect();
    auto direction_rows = directions.active();
    std::ostringstream b;
    b << "<div class='page-head'><h1>Добавить свою стажировку</h1><a class='btn btn-light' href='/applications'>К доске</a></div>"
      << "<form class='admin-form user-internship-form' method='post' action='/applications/new-internship'>"
      << "<label>Название</label><input name='title' required placeholder='Например, Backend Intern'>"
      << "<label>Компания</label><div class='stacked-field'><select name='company_id' id='companyChoice'>";
    for (const auto& c : company_rows) b << "<option value='" << c.at("id") << "'>" << htmlEscape(c.at("name")) << "</option>";
    b << "<option value='custom'>Другая компания</option></select><input class='is-hidden' id='customCompanyName' name='company_name' placeholder='Название новой компании'></div>"
      << "<label>Город</label><input name='city' placeholder='Москва, Пермь или удалённо'>"
      << "<label>Формат</label><select name='work_format'><option value='remote'>Удалённо</option><option value='hybrid'>Гибрид</option><option value='office'>Офис</option></select>"
      << "<label>Направление</label><div class='stacked-field'><select name='direction'>" << directionOptions(direction_rows, "frontend", false) << "<option value='custom'>Свое направление</option></select><input name='custom_direction' placeholder='Например, embedded'></div>"
      << "<label>Тип занятости</label><input name='employment_type' value='Стажировка'>"
      << "<label>Ссылка</label><input name='source_url' type='url' placeholder='https://...'>"
      << "<label>Описание</label><textarea name='description' rows='6' placeholder='Что это за стажировка, требования и условия'></textarea>"
      << "<button class='btn' type='submit'>Добавить на доску</button></form>"
      << "<script>(()=>{const select=document.getElementById('companyChoice');const input=document.getElementById('customCompanyName');if(!select||!input)return;const sync=()=>{const custom=select.value==='custom';input.classList.toggle('is-hidden',!custom);input.required=custom;if(custom)input.focus();};select.addEventListener('change',sync);sync();})();</script>";
    Response response;
    UserRepository users(db_);
    Row user = users.findById(user_id_);
    response.body = renderer_.layout("Своя стажировка | InternStart", b.str(), false, "applications", field(user, "email"));
    return response;
}

Response ApplicationController::board() const {
    ApplicationRepository applications(db_);
    // Подачи пользователя для канбан-доски
    auto rows = applications.board(user_id_);
    const std::vector<ApplicationStatus> statuses = {
        ApplicationStatus::WantToApply,
        ApplicationStatus::Applied,
        ApplicationStatus::Test,
        ApplicationStatus::Interview,
        ApplicationStatus::Offer,
        ApplicationStatus::Rejected
    };
    std::ostringstream b;
    b << "<section class='tracker-head'><h1>Мои подачи</h1><div class='tracker-top-actions'>"
      << "<a class='btn' href='/applications/new-internship'>Добавить свою стажировку</a>"
      << "<a class='btn btn-light' href='/applications?view=archive'>Архив (0)</a></div>"
      << "<p class='muted'>Перетаскивайте карточки между колонками, чтобы обновлять этап отклика.</p></section>"
      << "<section class='archive-dropzone' id='archiveDropzone' data-status='archived'>Перетащите карточку сюда, чтобы отправить в архив</section>"
      << "<section class='kanban-board' id='kanbanBoard'>";
    for (ApplicationStatus status : statuses) {
        int count = 0;
        const std::string status_text = toString(status);
        for (const auto& row : rows) if (row.at("status") == status_text) count++;
        b << "<div class='kanban-column' data-status='" << status_text << "'><div class='kanban-column-head'><h2>" << label(status) << "</h2><span class='kanban-count'>" << count << "</span></div><div class='kanban-list' data-status='" << status_text << "'>";
        for (const auto& row : rows) {
            if (row.at("status") != status_text) continue;
            b << "<article class='kanban-card company-themed' data-app-id='" << row.at("id") << "' draggable='true'>"
              << "<a class='kanban-card-link' href='/applications/" << row.at("id") << "/edit'><h3>" << htmlEscape(row.at("internship_title")) << "</h3>"
              << "<p class='muted'>";
            if (row.at("attempt_count") != "1") b << "<strong>Подача №" << htmlEscape(row.at("attempt_number")) << "</strong> · ";
            b << htmlEscape(row.at("company_display_name")) << "</p>";
            if (row.at("marker_enabled") == "1") {
                if (row.at("stage_completed") == "1") {
                    b << "<p class='muted kanban-meta-line'><span class='stage-pill stage-pill-done'>Выполнено</span></p>";
                } else if (!row.at("next_step_date").empty()) {
                    b << "<p class='kanban-due-date'>Выполнить: " << htmlEscape(displayDate(row.at("next_step_date"))) << "</p>";
                } else {
                    b << "<p class='muted kanban-meta-line'><span class='stage-pill stage-pill-pending'>Не выполнено</span></p>";
                }
            }
            // Задача по текущему этапу
            if (!row.at("task_note").empty()) {
                b << "<p class='kanban-task-note'>" << htmlEscape(row.at("task_note")) << "</p>";
            }
            b << "</a></article>";
        }
        b << "</div></div>";
    }
    b << "</section>";
    b << "<script>(()=>{const board=document.getElementById('kanbanBoard');const archiveDropzone=document.getElementById('archiveDropzone');if(!board)return;let draggedCard=null;const updateCounter=(column)=>{const count=column.querySelectorAll('.kanban-card').length;const node=column.querySelector('.kanban-count');if(node)node.textContent=String(count);};const syncStatus=async(appId,status)=>{const response=await fetch(`/applications/${appId}/move`,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({status})});if(!response.ok)throw new Error('Не удалось обновить статус.');};const moveToArchive=async(appId)=>{const response=await fetch(`/applications/${appId}/archive-move`,{method:'POST',headers:{'Content-Type':'application/json'}});return response.ok;};board.querySelectorAll('.kanban-card').forEach((card)=>{card.addEventListener('dragstart',()=>{draggedCard=card;card.classList.add('dragging');});card.addEventListener('dragend',()=>{card.classList.remove('dragging');draggedCard=null;});});board.querySelectorAll('.kanban-list').forEach((list)=>{list.addEventListener('dragover',(event)=>{event.preventDefault();list.classList.add('drag-over');});list.addEventListener('dragleave',()=>list.classList.remove('drag-over'));list.addEventListener('drop',async(event)=>{event.preventDefault();list.classList.remove('drag-over');if(!draggedCard)return;const fromColumn=draggedCard.closest('.kanban-column');const toColumn=list.closest('.kanban-column');const newStatus=list.dataset.status;const appId=draggedCard.dataset.appId;list.appendChild(draggedCard);updateCounter(fromColumn);updateCounter(toColumn);try{await syncStatus(appId,newStatus);}catch(error){fromColumn.querySelector('.kanban-list').appendChild(draggedCard);updateCounter(fromColumn);updateCounter(toColumn);alert(error.message);}});});if(archiveDropzone){archiveDropzone.addEventListener('dragover',(event)=>{event.preventDefault();archiveDropzone.classList.add('drag-over');});archiveDropzone.addEventListener('dragleave',()=>archiveDropzone.classList.remove('drag-over'));archiveDropzone.addEventListener('drop',async(event)=>{event.preventDefault();archiveDropzone.classList.remove('drag-over');if(!draggedCard)return;const fromColumn=draggedCard.closest('.kanban-column');const appId=draggedCard.dataset.appId;if(await moveToArchive(appId)){draggedCard.remove();updateCounter(fromColumn);}else alert('Не удалось перенести в архив.');});}})();</script>";
    Response response;
    UserRepository users(db_);
    Row user = users.findById(user_id_);
    response.body = renderer_.layout("InternStart | Мои подачи", b.str(), false, "applications", field(user, "email"));
    return response;
}

Response ApplicationController::addToTracker(int internship_id) const {
    InternshipRepository internships(db_);
    if (internships.publicDetail(internship_id).empty()) {
        Response response;
        response.status = 404;
        response.body = "Not found";
        return response;
    }
    ApplicationRepository applications(db_);
    applications.createAttempt(internship_id, user_id_, ApplicationStatus::WantToApply);
    return redirectTo("/applications");
}

Response ApplicationController::edit(int attempt_id, const Request& request) const {
    ApplicationRepository applications(db_);
    Row application = applications.findAttempt(attempt_id, user_id_);
    if (application.empty()) {
        Response response;
        response.status = 404;
        UserRepository users(db_);
        Row user = users.findById(user_id_);
        response.body = renderer_.layout("404", "<h1>Подача не найдена</h1>", false, "applications", field(user, "email"));
        return response;
    }

    if (request.method == "POST") {
        applications.updateAttemptDetails(attempt_id, user_id_, request.form);
        return redirectTo("/applications/" + std::to_string(attempt_id) + "/edit");
    }

    std::vector<Row> attempts = applications.attemptsForInternship(std::stoi(application["internship_id"]), user_id_);
    int current_number = 1;
    for (size_t i = 0; i < attempts.size(); ++i) {
        if (attempts[i]["id"] == application["id"]) current_number = static_cast<int>(i + 1);
    }

    std::ostringstream b;
    b << "<section class='application-layout'>"
      << "<aside class='application-aside detail'>"
      << "<div class='application-aside-head'><h1>Подача №" << current_number << "</h1><span class='status-chip'>" << label(applicationStatusFromString(application["status"])) << "</span></div>"
      << "<p class='application-role'>" << htmlEscape(application["internship_title"]) << "</p>"
      << "<p class='muted'>" << htmlEscape(application["company_display_name"]) << "</p>"
      << "<div class='actions application-side-actions'><a class='btn' href='/applications'>К доске</a><a class='btn btn-light' href='/internships/" << application["internship_id"] << "'>К стажировке</a><a class='btn btn-light' href='#edit-details'>Редактировать</a></div>"
      << "<dl class='application-facts application-facts-clean'>"
      << "<div><dt>Дата подачи</dt><dd>" << htmlEscape(displayDate(application["applied_at"])) << "</dd></div>"
      << "<div><dt>Дата выполнения</dt><dd>" << htmlEscape(displayDate(application["next_step_date"])) << "</dd></div>"
      << "<div><dt>Время</dt><dd>" << htmlEscape(displayTime(application["next_step_time"])) << "</dd></div>"
      << "<div><dt>Маркер выполнения</dt><dd>" << (application["marker_enabled"] != "0" ? "Показывается" : "Скрыт") << "</dd></div>"
      << "</dl>";
    if (!application["note"].empty()) {
        b << "<div class='application-note'><span>Заметка</span><p>" << htmlEscape(application["note"]) << "</p></div>";
    }
    b << "<div class='edit-modal' id='edit-details'><a class='edit-modal-backdrop' href='#' aria-label='Закрыть'></a>"
      << "<section class='edit-dialog'><div class='edit-dialog-head'><h2>Редактировать подачу</h2><a class='edit-close' href='#' aria-label='Закрыть'>×</a></div>"
      << "<form method='post' class='application-modal-form'>"
      << "<label>Дата подачи<input type='date' name='applied_at' value='" << htmlEscape(application["applied_at"]) << "'></label>"
      << "<label>Дата выполнения<input type='date' name='next_step_date' value='" << htmlEscape(application["next_step_date"]) << "'></label>"
      << "<label>Время<input type='time' name='next_step_time' value='" << htmlEscape(application["next_step_time"]) << "'></label>"
      << "<label class='modal-check'><input type='hidden' name='marker_enabled' value='0'><input type='checkbox' name='marker_enabled' value='1' " << (application["marker_enabled"] != "0" ? "checked" : "") << "> <span>Показывать маркер выполнения этапа</span></label>"
      << "<label class='modal-wide'>Заметка<textarea name='note' rows='4' placeholder='Свободные заметки по отклику'>" << htmlEscape(application["note"]) << "</textarea></label>"
      << "<div class='modal-actions'><button class='btn' type='submit'>Сохранить</button><a class='btn btn-light' href='#'>Отмена</a>"
      << "<button class='btn btn-danger' type='submit' formaction='/applications/" << attempt_id << "/delete' formmethod='post' onclick=\"return confirm('Удалить стажировку из канбан-доски?');\">Удалить</button></div>"
      << "</form></section></div></aside>";

    b << "<section class='application-history detail'><div class='history-head'><h2>История подач</h2><p class='muted'>Главный фокус: этапы, дедлайны, комментарии и прогресс.</p></div>";
    for (size_t i = 0; i < attempts.size(); ++i) {
        const Row& attempt = attempts[i];
        bool is_current = attempt.at("id") == application["id"];
        auto history = applications.historyForAttempt(std::stoi(attempt.at("id")));
        std::string checkbox_event_id;
        for (auto it = history.rbegin(); it != history.rend(); ++it) {
            if (it->at("event_type") != "completion") {
                checkbox_event_id = it->at("id");
                break;
            }
        }
        b << "<details class='attempt-panel' " << (is_current ? "open" : "") << "><summary><strong>Подача №" << (i + 1) << "</strong><span class='muted'>Текущий этап: " << label(applicationStatusFromString(attempt.at("status"))) << "</span></summary>";
        b << "<div class='attempt-actions'>";
        if (is_current) {
            b << "<form method='post' action='/applications/" << attempt.at("id") << "/delete' onsubmit=\"return confirm('Удалить Подачу №" << (i + 1) << "?');\"><button class='btn btn-danger btn-small' type='submit'>Удалить подачу</button></form>";
        } else {
            b << "<span class='muted'>Только просмотр (неактивная подача)</span>";
        }
        b << "</div>";
        if (is_current) {
            b << "<form method='post' action='/applications/" << attempt.at("id") << "/events' class='attempt-event-form'><button class='btn btn-light btn-small' type='submit'>Добавить этап</button></form>";
        }
        b << "<ul class='attempt-timeline'>";
        for (const auto& event : history) {
            b << "<li class='timeline-item'><div class='timeline-item-head'>";
            std::string kind = "Этап";
            if (event.at("event_type") == "create") kind = "Создано";
            if (event.at("event_type") == "completion") kind = "Выполнение";
            b << "<span class='timeline-kind'>" << kind << "</span><span class='timeline-time mono'>" << htmlEscape(event.at("created_at")) << "</span></div><div class='timeline-item-body'>";
            if (event.at("event_type") == "completion") {
                b << "<strong>" << htmlEscape(event.at("event_note").empty() ? "Статус выполнения этапа изменен" : event.at("event_note")) << "</strong>";
            } else if (!event.at("from_status").empty() && !event.at("to_status").empty()) {
                b << "<strong>" << label(applicationStatusFromString(event.at("from_status"))) << " → " << label(applicationStatusFromString(event.at("to_status"))) << "</strong>";
            } else if (event.at("event_type") == "create") {
                b << "<strong>Подача создана</strong>";
            }
            b << "<div class='timeline-event-meta'>";
            if (!event.at("stage_deadline_date").empty()) {
                b << "<span class='timeline-meta-chip'>Дата: " << htmlEscape(event.at("stage_deadline_date")) << (!event.at("stage_deadline_time").empty() ? " " + htmlEscape(event.at("stage_deadline_time")) : "") << "</span>";
            }
            if (is_current && attempt.at("marker_enabled") == "1" && event.at("id") == checkbox_event_id) {
                bool done = attempt.at("stage_completed") == "1";
                b << "<span class='timeline-meta-chip timeline-meta-chip-check'><form method='post' action='/applications/" << attempt.at("id") << "/events/" << event.at("id") << "/completion' class='timeline-complete-form'>"
                  << "<input type='hidden' name='stage_completed' value='" << (done ? "0" : "1") << "'><label class='timeline-complete-toggle'><input type='checkbox' " << (done ? "checked" : "") << " onchange='this.form.submit()'><span>Этап выполнен</span></label></form></span>";
            }
            b << "</div>";
            if (is_current) {
                b << "<form method='post' action='/applications/" << attempt.at("id") << "/events/" << event.at("id") << "/comment' class='timeline-inline-comment-form' id='event-comment-" << event.at("id") << "'>"
                  << "<textarea name='event_note' rows='1' maxlength='500' class='timeline-inline-comment-input' placeholder='Задача по этому этапу'>" << htmlEscape(event.at("event_note")) << "</textarea>"
                  << "<div class='timeline-inline-actions'><button class='btn btn-light btn-small' type='submit'>Сохранить</button></div></form>";
            } else if (!event.at("event_note").empty()) {
                b << "<div class='timeline-inline-comment-read'>" << htmlEscape(event.at("event_note")) << "</div>";
            }
            b << "</div></li>";
        }
        b << "</ul></details>";
    }
    b << "</section></section>";
    b << "<script>(()=>{document.querySelectorAll('.timeline-comment-toggle').forEach((toggle)=>{toggle.addEventListener('click',()=>{const target=document.getElementById(toggle.getAttribute('data-target'));if(!target)return;target.classList.toggle('is-hidden');const textarea=target.querySelector('.timeline-inline-comment-input');if(textarea)textarea.focus();});});})();</script>";

    Response response;
    UserRepository users(db_);
    Row user = users.findById(user_id_);
    response.body = renderer_.layout("Редактирование подачи | InternStart", b.str(), false, "applications", field(user, "email"));
    return response;
}

Response ApplicationController::moveForm(int attempt_id, const Request& request) const {
    ApplicationRepository applications(db_);
    applications.moveAttempt(attempt_id, user_id_, applicationStatusFromString(field(request.form, "status", "want_to_apply")));
    return redirectTo("/applications");
}

Response ApplicationController::addEvent(int attempt_id) const {
    ApplicationRepository applications(db_);
    applications.addStageEvent(attempt_id, user_id_);
    return redirectTo("/applications/" + std::to_string(attempt_id) + "/edit");
}

Response ApplicationController::updateEventComment(int attempt_id, int event_id, const Request& request) const {
    ApplicationRepository applications(db_);
    applications.updateEventComment(attempt_id, user_id_, event_id, field(request.form, "event_note"));
    return redirectTo("/applications/" + std::to_string(attempt_id) + "/edit");
}

Response ApplicationController::updateEventCompletion(int attempt_id, int event_id, const Request& request) const {
    ApplicationRepository applications(db_);
    applications.updateEventCompletion(attempt_id, user_id_, event_id, field(request.form, "stage_completed") == "1");
    return redirectTo("/applications/" + std::to_string(attempt_id) + "/edit");
}

Response ApplicationController::deleteAttempt(int attempt_id) const {
    ApplicationRepository applications(db_);
    applications.deleteAttempt(attempt_id, user_id_);
    return redirectTo("/applications");
}

Response ApplicationController::moveJson(int attempt_id, const Request& request) const {
    std::smatch match;
    std::string status = "want_to_apply";
    if (std::regex_search(request.body, match, std::regex("\"status\"\\s*:\\s*\"([^\"]+)\""))) status = match[1];
    ApplicationRepository applications(db_);
    applications.moveAttempt(attempt_id, user_id_, applicationStatusFromString(status));
    Response response;
    response.content_type = "application/json";
    response.body = "{\"ok\":true}";
    return response;
}

Response ApplicationController::archiveJson(int attempt_id) const {
    ApplicationRepository applications(db_);
    applications.moveAttempt(attempt_id, user_id_, ApplicationStatus::Archived);
    Response response;
    response.content_type = "application/json";
    response.body = "{\"ok\":true,\"status\":\"archived\"}";
    return response;
}

AdminController::AdminController(Database& db, const PageRenderer& renderer) : db_(db), renderer_(renderer) {}

bool AdminController::isAdmin(const Request& request) const {
    auto it = request.headers.find("cookie");
    return it != request.headers.end() && it->second.find("admin_auth=1") != std::string::npos;
}

Response AdminController::requireAdmin(const Request& request) const {
    if (isAdmin(request)) return Response{};
    return redirectTo("/admin/login");
}

Response AdminController::login(const Request& request) const {
    bool invalid_credentials = false;
    if (request.method == "POST") {
        const std::string admin_username = envOr("ADMIN_USERNAME", "admin");
        const std::string admin_password = envOr("ADMIN_PASSWORD", "change_me");
        if (field(request.form, "username") == admin_username && field(request.form, "password") == admin_password) {
            Response response = redirectTo("/admin/accounts");
            response.headers.push_back("Set-Cookie: admin_auth=1; Path=/; HttpOnly; SameSite=Lax");
            return response;
        }
        invalid_credentials = true;
    }
    std::ostringstream b;
    b << "<section class='admin-login-page'><div class='admin-login-card'>"
      << "<div><p class='eyebrow'>InternStart Admin</p><h1>Вход в админ-панель</h1><p class='muted'>Управление компаниями, стажировками и направлениями без регистрации.</p></div>"
      << "<form method='post' class='auth-form admin-login-form' autocomplete='off'>";
    if (invalid_credentials) {
        b << "<p class='auth-message auth-message-error'>Пароль неверный.</p>";
    }
    b
      << "<label>Логин<input name='username' autocomplete='off' required></label>"
      << "<label>Пароль<input name='password' type='password' autocomplete='new-password' required></label>"
      << "<button class='btn btn-auth'>Войти</button></form></div></section>";
    Response response;
    response.body = renderer_.layout("Админка", b.str(), true, "landing");
    return response;
}

Response AdminController::accounts(const Request& request) const {
    Response guard = requireAdmin(request);
    if (guard.status == 302) return guard;
    UserRepository users(db_);
    auto rows = users.all();
    std::ostringstream b;
    b << "<div class='page-head'><h1>Аккаунты</h1><a class='btn' href='/admin/accounts/new'>+ Профиль</a></div>"
      << "<div class='table-wrap'><table><thead><tr><th>ID</th><th>Почта</th><th>Создан</th></tr></thead><tbody>";
    for (const auto& row : rows) {
        b << "<tr><td>" << row.at("id") << "</td><td>" << htmlEscape(row.at("email")) << "</td><td>" << htmlEscape(row.at("created_at")) << "</td></tr>";
    }
    b << "</tbody></table></div>";
    Response response;
    response.body = renderer_.layout("Админка | Аккаунты", b.str(), true, "accounts");
    return response;
}

Response AdminController::newAccount(const Request& request) const {
    Response guard = requireAdmin(request);
    if (guard.status == 302) return guard;
    if (request.method == "POST") {
        UserRepository users(db_);
        if (!field(request.form, "email").empty() && !field(request.form, "password").empty()) {
            users.create(field(request.form, "email"), field(request.form, "password"));
        }
        return redirectTo("/admin/accounts");
    }
    std::ostringstream b;
    b << "<div class='page-head'><h1>Новый профиль</h1><a class='btn btn-light' href='/admin/accounts'>Назад</a></div>"
      << "<form class='admin-form' method='post' action='/admin/accounts/new'>"
      << "<label>Почта</label><input name='email' type='email' required>"
      << "<label>Пароль</label><input name='password' type='password' required>"
      << "<button class='btn'>Создать профиль</button></form>";
    Response response;
    response.body = renderer_.layout("Админка | Новый профиль", b.str(), true, "accounts");
    return response;
}

Response AdminController::internships(const Request& request) const {
    Response guard = requireAdmin(request);
    if (guard.status == 302) return guard;
    InternshipRepository internships(db_);
    auto rows = internships.adminList();
    std::ostringstream b;
    b << "<div class='page-head'><h1>Стажировки</h1><div class='actions'><a class='btn' href='/admin/internships/new'>+ Новая</a><a class='btn btn-light' href='/admin/companies'>Компании</a><a class='btn btn-light' href='/admin/imports'>Импорт</a></div></div><div class='table-wrap'><table><thead><tr><th>ID</th><th>Должность</th><th>Компания</th><th>Публикация</th><th>Проверка</th><th></th></tr></thead><tbody>";
    for (const auto& row : rows) {
        b << "<tr><td>" << row.at("id") << "</td><td>" << htmlEscape(row.at("title")) << "</td><td>" << htmlEscape(row.at("company_display_name")) << "</td><td>" << (row.at("is_published") == "1" ? "Опубликована" : "Черновик") << "</td><td>" << (row.at("last_verified_at").empty() ? "Не проверена" : "Проверена " + htmlEscape(row.at("last_verified_at").substr(0, 10))) << "</td><td class='table-actions'>"
          << "<a class='btn btn-small btn-light' href='/admin/internships/" << row.at("id") << "/edit'>Редактировать</a>"
          << "<form method='post' action='/admin/internships/" << row.at("id") << "/verify'><button class='btn btn-small btn-light'>Проверена</button></form>"
          << "<form method='post' action='/admin/internships/" << row.at("id") << "/toggle-publish'><button class='btn btn-small'>Переключить</button></form>"
          << "<form method='post' action='/admin/internships/" << row.at("id") << "/delete'><button class='btn btn-small btn-danger'>Удалить</button></form></td></tr>";
    }
    b << "</tbody></table></div>";
    Response response;
    response.body = renderer_.layout("Админка | Стажировки", b.str(), true, "admin");
    return response;
}

std::string AdminController::internshipForm(const Row& row, const std::string& action) const {
    CompanyRepository companies(db_);
    DirectionRepository directions(db_);
    auto company_rows = companies.activeForSelect();
    auto direction_rows = directions.active();
    auto val = [&](const std::string& key, const std::string& fallback = "") {
        auto it = row.find(key);
        return htmlEscape(it == row.end() || it->second.empty() ? fallback : it->second);
    };
    std::ostringstream b;
    b << "<form class='admin-form' method='post' action='" << action << "'>"
      << "<label>Название</label><input name='title' value='" << val("title") << "'>"
      << "<label>Компания</label><select name='company_id'>";
    for (const auto& c : company_rows) b << "<option value='" << c.at("id") << "'" << (c.at("id") == val("company_id") ? " selected" : "") << ">" << htmlEscape(c.at("name")) << "</option>";
    b << "</select><label>Город</label><input name='city' value='" << val("city", "Не указано") << "'>"
      << "<label>Формат</label><select name='work_format'><option value='office'>Офис</option><option value='remote'>Удалённо</option><option value='hybrid'>Гибрид</option></select>"
      << "<label>Направление</label><select name='direction'>" << directionOptions(direction_rows, field(row, "direction", "other")) << "</select>"
      << "<label>Тип занятости</label><input name='employment_type' value='" << val("employment_type", "Не указано") << "'>"
      << "<label>Краткое описание</label><textarea name='short_description'>" << val("short_description") << "</textarea>"
      << "<label>Полное описание</label><textarea name='full_description'>" << val("full_description") << "</textarea>"
      << "<label>Источник</label><input name='source_url' value='" << val("source_url") << "'>"
      << "<label>Статус</label><select name='status'><option value='open'>Набор открыт</option><option value='closing_soon'>Скоро закрывается</option><option value='closed'>Завершен</option></select>"
      << "<label><input type='checkbox' name='is_published' value='1' checked> Опубликована</label><button class='btn'>Сохранить</button></form>";
    return b.str();
}

Response AdminController::newInternship(const Request& request) const {
    Response guard = requireAdmin(request);
    if (guard.status == 302) return guard;
    if (request.method == "POST") {
        InternshipRepository internships(db_);
        internships.save(request.form);
        return redirectTo("/admin/internships");
    }
    Response response;
    response.body = renderer_.layout("Новая стажировка", internshipForm({}, "/admin/internships/new"), true, "admin");
    return response;
}

Response AdminController::editInternship(int id, const Request& request) const {
    Response guard = requireAdmin(request);
    if (guard.status == 302) return guard;
    InternshipRepository internships(db_);
    if (request.method == "POST") {
        internships.save(request.form, id);
        return redirectTo("/admin/internships");
    }
    Response response;
    response.body = renderer_.layout("Редактирование", internshipForm(internships.findById(id), request.path), true, "admin");
    return response;
}

Response AdminController::togglePublish(int id, const Request& request) const {
    Response guard = requireAdmin(request);
    if (guard.status == 302) return guard;
    InternshipRepository internships(db_);
    internships.togglePublish(id);
    return redirectTo("/admin/internships");
}

Response AdminController::verifyInternship(int id, const Request& request) const {
    Response guard = requireAdmin(request);
    if (guard.status == 302) return guard;
    InternshipRepository internships(db_);
    internships.markVerified(id);
    return redirectTo("/admin/internships");
}

Response AdminController::deleteInternship(int id, const Request& request) const {
    Response guard = requireAdmin(request);
    if (guard.status == 302) return guard;
    InternshipRepository internships(db_);
    internships.remove(id);
    return redirectTo("/admin/internships");
}

Response AdminController::companies(const Request& request) const {
    Response guard = requireAdmin(request);
    if (guard.status == 302) return guard;
    CompanyRepository companies(db_);
    auto rows = companies.adminList();
    std::ostringstream b;
    b << "<div class='page-head'><h1>Компании</h1><a class='btn' href='/admin/companies/new'>+ Новая</a></div><div class='table-wrap'><table><thead><tr><th>ID</th><th>Название</th><th>Slug</th><th>Активна</th><th>Стажировок</th><th></th></tr></thead><tbody>";
    for (const auto& row : rows) {
        b << "<tr><td>" << row.at("id") << "</td><td>" << htmlEscape(row.at("name")) << "</td><td>" << htmlEscape(row.at("slug")) << "</td><td>" << (row.at("is_active") == "1" ? "Да" : "Нет") << "</td><td>" << row.at("internships_count") << "</td><td class='table-actions'><a class='btn btn-small btn-light' href='/admin/companies/" << row.at("id") << "/edit'>Редактировать</a>"
          << "<form method='post' action='/admin/companies/" << row.at("id") << "/delete' onsubmit=\"return confirm('Удалить компанию?');\"><button class='btn btn-small btn-danger'>Удалить</button></form></td></tr>";
    }
    b << "</tbody></table></div>";
    Response response;
    response.body = renderer_.layout("Админка | Компании", b.str(), true, "companies");
    return response;
}

std::string AdminController::companyForm(const Row& row, const std::string& action) const {
    auto val = [&](const std::string& key, const std::string& fallback = "") {
        auto it = row.find(key);
        return htmlEscape(it == row.end() || it->second.empty() ? fallback : it->second);
    };
    std::ostringstream b;
    b << "<form class='admin-form' method='post' action='" << action << "'>"
      << "<label>Название</label><input name='name' value='" << val("name") << "'>"
      << "<label>Сайт</label><input name='website_url' value='" << val("website_url") << "'>"
      << "<label>Карьера</label><input name='career_url' value='" << val("career_url") << "'>"
      << "<label>Описание</label><textarea name='description'>" << val("description") << "</textarea>"
      << "<label>О стажировках</label><textarea name='internship_info'>" << val("internship_info") << "</textarea>"
      << "<label>Особенности подачи</label><textarea name='application_notes'>" << val("application_notes") << "</textarea>"
      << "<label>Цвет</label><div class='color-picker-row'><input class='color-picker-square' name='accent_color' type='color' value='" << val("accent_color", "#0e7490") << "'></div>"
      << "<button class='btn'>Сохранить</button></form>";
    return b.str();
}

Response AdminController::newCompany(const Request& request) const {
    Response guard = requireAdmin(request);
    if (guard.status == 302) return guard;
    if (request.method == "POST") {
        CompanyRepository companies(db_);
        companies.save(request.form);
        return redirectTo("/admin/companies");
    }
    Response response;
    response.body = renderer_.layout("Новая компания", companyForm({}, "/admin/companies/new"), true, "companies");
    return response;
}

Response AdminController::editCompany(int id, const Request& request) const {
    Response guard = requireAdmin(request);
    if (guard.status == 302) return guard;
    CompanyRepository companies(db_);
    if (request.method == "POST") {
        companies.save(request.form, id);
        return redirectTo("/admin/companies");
    }
    Response response;
    response.body = renderer_.layout("Редактирование компании", companyForm(companies.findById(id), request.path), true, "companies");
    return response;
}

Response AdminController::deleteCompany(int id, const Request& request) const {
    Response guard = requireAdmin(request);
    if (guard.status == 302) return guard;
    CompanyRepository companies(db_);
    companies.remove(id);
    return redirectTo("/admin/companies");
}

Response AdminController::directions(const Request& request) const {
    Response guard = requireAdmin(request);
    if (guard.status == 302) return guard;
    DirectionRepository directions(db_);
    if (request.method == "POST") {
        directions.save(request.form);
        return redirectTo("/admin/directions");
    }
    auto rows = directions.all();
    std::ostringstream b;
    b << "<div class='page-head'><h1>Направления</h1><a class='btn btn-light' href='/admin/internships'>К стажировкам</a></div>"
      << "<form class='admin-form compact-admin-form' method='post' action='/admin/directions'>"
      << "<label>Новое направление</label><input name='name' placeholder='Например, devops' required>"
      << "<button class='btn'>Добавить</button></form>"
      << "<div class='table-wrap directions-table'><table><thead><tr><th>ID</th><th>Название</th><th>Статус</th><th></th></tr></thead><tbody>";
    for (const auto& row : rows) {
        b << "<tr><td>" << row.at("id") << "</td><td>" << htmlEscape(row.at("name")) << "</td><td>" << (row.at("is_active") == "1" ? "Активно" : "Скрыто") << "</td>"
          << "<td class='table-actions'><form method='post' action='/admin/directions/" << row.at("id") << "/toggle'><button class='btn btn-small btn-light'>Переключить</button></form></td></tr>";
    }
    b << "</tbody></table></div>";
    Response response;
    response.body = renderer_.layout("Админка | Направления", b.str(), true, "directions");
    return response;
}

Response AdminController::toggleDirection(int id, const Request& request) const {
    Response guard = requireAdmin(request);
    if (guard.status == 302) return guard;
    DirectionRepository directions(db_);
    directions.toggle(id);
    return redirectTo("/admin/directions");
}

Response AdminController::imports(const Request& request) const {
    Response guard = requireAdmin(request);
    if (guard.status == 302) return guard;
    std::ostringstream b;
    b << "<div class='page-head'><h1>Импорт</h1></div><div class='card'><p><strong>Импорт недоступен</strong></p>"
      << "<p>Сетевой импорт удален из текущего backend. Его можно вернуть позже отдельным сервисным слоем.</p></div>";
    Response response;
    response.body = renderer_.layout("Админка | Импорт", b.str(), true, "imports");
    return response;
}

} // namespace internstart
