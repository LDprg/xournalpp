#include "ColorPalette.h"

#include <fstream>
#include <limits>
#include <sstream>

#include <gtk/gtk.h>

#include "util/StringUtils.h"
#include "util/i18n.h"


Palette::Palette(fs::path path): filepath{std::move(path)}, header{}, namedColors{} {}

void Palette::load() {
    if (!fs::exists(this->filepath))
        throw std::invalid_argument(
                FS(FORMAT_STR("The palette file {1} does not exist.") % this->filepath.filename().u8string()));
    int lineNumber{};
    header.clear();
    namedColors.clear();

    std::ifstream gplFile{filepath};
    std::string line;

    if (gplFile.is_open()) {
        getline(gplFile, line);
        lineNumber++;
        // parse standard header line
        parseFirstGimpPaletteLine(line);
        /*
         * attempt parsing line by line as
         * either header, color, or fallback
         */
        while (getline(gplFile, line)) {
            lineNumber++;
            parseCommentLine(line) || parseHeaderLine(line) || parseColorLine(line) || parseLineFallback(lineNumber);
        }
        if (namedColors.size() < 1)
            throw std::invalid_argument("Your Palettefile has no parsable color. It needs at least one!");
    }
}

auto Palette::load_default() -> void {
    std::stringstream defaultFile{default_palette()};
    std::string line;

    getline(defaultFile, line);
    if (!parseFirstGimpPaletteLine(line))
        g_error("The default file was mallformed. This should never happen!");
    while (getline(defaultFile, line)) { parseHeaderLine(line) || parseColorLine(line); }
}

auto Palette::parseFirstGimpPaletteLine(const std::string& line) const -> bool {
    if (StringUtils::trim(line) != "GIMP Palette") {
        throw std::invalid_argument(".gpl file needs to start with \"GIMP Palette\" in the "
                                    "first line");
    }
    return true;
}

auto Palette::parseHeaderLine(const std::string& line) -> bool {
    Header headerEntry;
    std::istringstream iss{line};
    if (iss >> headerEntry) {
        header[headerEntry.getAttribute()] = headerEntry.getValue();
        return true;
    }
    return false;
}

auto Palette::parseColorLine(const std::string& line) -> bool {
    NamedColor color{namedColors.size()};
    std::istringstream iss{line};
    if (iss >> color) {
        namedColors.emplace_back(std::move(color));
        return true;
    }
    return false;
}

auto Palette::parseCommentLine(const std::string& line) const -> bool {
    if (line.front() == '#')
        return true;
    else
        return false;
}

auto Palette::parseLineFallback(int lineNumber) const -> const bool {
    throw std::invalid_argument(FS(FORMAT_STR("The line {1} is malformed.") % lineNumber));
}

auto Palette::getColorAt(size_t i) const -> NamedColor const& {
    if (i >= namedColors.size()) {
        i = i % namedColors.size();
        g_warning("There are more Coloritems in the Toolbar than your Palette defines.\n"
                  "Hence, cycling through palette from the beginning.");
    }

    return namedColors.at(i);
}

auto Palette::size() const -> size_t { return namedColors.size(); }


auto Palette::default_palette() -> const std::string {
    std::stringstream d{};
    d << "GIMP Palette" << std::endl;
    d << "Name: Xournal Default Palette" << std::endl;
    d << "#" << std::endl;
    d << 0 << " " << 0 << " " << 0 << " "
      << "Black" << std::endl;
    d << 0 << " " << 128 << " " << 0 << " "
      << "Green" << std::endl;
    d << 0 << " " << 192 << " " << 255 << " "
      << "Light Blue" << std::endl;
    d << 0 << " " << 255 << " " << 0 << " "
      << "Light Green" << std::endl;
    d << 51 << " " << 51 << " " << 204 << " "
      << "Blue" << std::endl;
    d << 128 << " " << 128 << " " << 128 << " "
      << "Gray" << std::endl;
    d << 255 << " " << 0 << " " << 0 << " "
      << "Red" << std::endl;
    d << 255 << " " << 0 << " " << 255 << " "
      << "Magenta" << std::endl;
    d << 255 << " " << 128 << " " << 0 << " "
      << "Orange" << std::endl;
    d << 255 << " " << 255 << " " << 0 << " "
      << "Yellow" << std::endl;
    d << 255 << " " << 255 << " " << 255 << " "
      << "White" << std::endl;
    return d.str();
}

void Palette::create_default(fs::path filepath) {
    std::ofstream myfile{filepath};
    myfile << default_palette();
}

auto Header::getAttribute() const -> std::string { return this->attribute; };
auto Header::getValue() const -> std::string { return this->value; };


auto operator>>(std::istream& str, Header& header) -> std::istream& {
    /*
     * inspired by
     * https://codereview.stackexchange.com/questions/38879/parsing-text-file-in-c
     */
    std::string line;
    Header tmp;
    if (std::getline(str, line)) {
        std::istringstream iss{line};
        if (getline(iss, tmp.attribute, ':') && getline(iss, tmp.value, '\n')) {
            tmp.attribute = StringUtils::trim(tmp.attribute);
            tmp.value = StringUtils::trim(tmp.value);

            // parsing was successful hence results can be stored permanently
            header = std::move(tmp);
        } else {
            /*
             * One operation failed.
             * So set the state on the main stream
             * to indicate failure.
             */
            str.setstate(std::ios::failbit);
        }
    }
    return str;
}

auto Palette::parseErrorDialog(const std::exception& e) const -> void {

    std::stringstream msg_stream{};
    msg_stream << "There has been a problem parsing the color palette file at " << filepath.c_str() << "\n\n";
    msg_stream << "What happened:\n" << e.what() << std::endl;
    msg_stream << "What to do:\n";
    msg_stream << "Please fix your palette file, or rename it so xournalpp creates a new default palette file "
                  "for you. This file can then be used as a template.\n";
    msg_stream << "What will happen now:\nThe application will start with the default color palette.";

    GtkWidget* dialog = gtk_message_dialog_new(nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s",
                                               msg_stream.str().c_str());

    gtk_dialog_add_button(GTK_DIALOG(dialog), _("OK"), 1);

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    g_warning(msg_stream.str().c_str());
}