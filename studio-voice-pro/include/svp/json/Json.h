// Studio Voice Pro — module json : analyseur et écrivain JSON maison.
// Aucune dépendance externe (pas de nlohmann/json, pas de RapidJSON) : le
// moteur doit rester autonome et compilable partout sans téléchargement.
//
// Deux points sont critiques pour le format de projet (docs/PROJECT-FORMAT.md) :
//   1. l'écriture suit un ORDRE STABLE (l'ordre d'appel des méthodes du
//      Writer), donc un `diff` entre deux projets reste lisible ;
//   2. les nombres ne passent jamais par une conversion textuelle à précision
//      fixe : les entiers 64 bits sont écrits en décimal pur et les flottants
//      avec std::to_chars (représentation la plus courte qui, relue par
//      std::from_chars, redonne EXACTEMENT le même double).
// Ces deux fonctions de <charconv> n'utilisent pas la locale du programme :
// un poste configuré en français n'écrira jamais « -6,5 » (JSON invalide).
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace svp::json {

enum class Type { Null, Bool, Number, String, Array, Object };

/** Valeur JSON en mémoire. Un nombre entier (texte source sans point ni
    exposant) est mémorisé aussi en `int64_t` : les positions en échantillons
    ne transitent jamais par un double (au-delà de 2^53 un double perd des
    unités — un projet de plusieurs jours d'audio y arrive). */
struct Value {
    Type type = Type::Null;
    bool boolean = false;
    bool integral = false;
    int64_t integer = 0;
    double number = 0.0;
    std::string text;                                   // si String
    std::vector<Value> items;                            // si Array
    std::vector<std::pair<std::string, Value>> members;  // si Object (ordre source)

    [[nodiscard]] bool isNull() const noexcept { return type == Type::Null; }
    [[nodiscard]] bool isBool() const noexcept { return type == Type::Bool; }
    [[nodiscard]] bool isNumber() const noexcept { return type == Type::Number; }
    [[nodiscard]] bool isString() const noexcept { return type == Type::String; }
    [[nodiscard]] bool isArray() const noexcept { return type == Type::Array; }
    [[nodiscard]] bool isObject() const noexcept { return type == Type::Object; }
    /** Nombre entier exact (vrai seulement si la source était un entier). */
    [[nodiscard]] bool isInteger() const noexcept { return type == Type::Number && integral; }

    /** Membre d'un objet, ou nullptr si absent / valeur non-objet. */
    [[nodiscard]] const Value* find(std::string_view key) const;
    /** Élément d'un tableau, ou nullptr hors bornes. */
    [[nodiscard]] const Value* item(std::size_t index) const;
    [[nodiscard]] std::size_t size() const noexcept;

    [[nodiscard]] bool boolOr(bool fallback) const noexcept;
    [[nodiscard]] double doubleOr(double fallback) const noexcept;
    [[nodiscard]] int64_t intOr(int64_t fallback) const noexcept;
    [[nodiscard]] std::string stringOr(std::string fallback = {}) const;

    static Value makeObject();
    static Value makeArray();
    static Value makeString(std::string text);
    static Value makeInt(int64_t value);
    static Value makeDouble(double value);
    static Value makeBool(bool value);
    /** Insère (ou remplace) un membre — utilisé par les tests et les migrations. */
    void set(std::string key, Value member);
};

struct ParseResult {
    bool ok = false;
    std::string error;  // message lisible, sans jamais lever d'exception
    Value value;
};

/** Analyse un document JSON. Ne lève jamais : toute erreur est renvoyée dans
    `error`, avec la ligne et la colonne. Refuse NaN/Infinity (hors JSON), les
    entiers débordant `int64_t` (perte de précision silencieuse) et toute
    donnée après la fin de la valeur racine. */
[[nodiscard]] ParseResult parse(std::string_view text);

/** Échappement d'une chaîne JSON (guillemets, antislash, caractères de
    contrôle). Les octets UTF-8 valides sont recopiés tels quels : le fichier
    est écrit en UTF-8, ce qui garde les accents lisibles dans un `diff`. */
[[nodiscard]] std::string escape(std::string_view text);

/** Écrivain JSON à ordre stable. On appelle les méthodes dans l'ordre exact
    où les champs doivent apparaître dans le fichier. */
class Writer {
public:
    explicit Writer(int indentWidth = 2) : indentWidth_(indentWidth) {}

    void beginObject();
    void endObject();
    void beginArray();
    void endArray();
    void key(std::string_view name);
    void valueString(std::string_view value);
    void valueInt(int64_t value);
    void valueDouble(double value);
    void valueBool(bool value);
    void valueNull();
    /** Écrit une sous-valeur déjà construite (objet/tableau), clés comprises. */
    void value(const Value& value);

    /** Texte complet, terminé par un saut de ligne (diff stable). */
    [[nodiscard]] std::string str() const;

private:
    struct Frame {
        bool isObject = false;
        bool hasMembers = false;
    };
    void startValue();
    void newlineIndent();
    void write(const Value& value);

    std::string out_;
    std::vector<Frame> stack_;
    std::string pendingKey_;
    bool hasPendingKey_ = false;
    int indentWidth_ = 2;
    bool nonFiniteSeen_ = false;

public:
    /** Vrai si une valeur non finie (NaN/∞) a été écrite — elle l'a été sous
        forme `null`. L'appelant doit refuser d'enregistrer un tel projet. */
    [[nodiscard]] bool wroteNonFinite() const noexcept { return nonFiniteSeen_; }
};

} // namespace svp::json
