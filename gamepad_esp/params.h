#ifndef _PARAMS_H_
#define _PARAMS_H_

Preferences preferences;                  // параметры EEPROM


//==================================================================================================
/*
Имя параметра - ограничено длиной в 15 символов. Представляет собой одно слово.
Оно же является ключом для поиска параметра и сохранения его в non-volatile memory (NVS) of the ESP32 to store data
012345678901234
Game_time
Activated_time
Bomb_time
Password
*/


// https://github.com/josephlarralde/arduino-esp-utils/blob/master/src/ESPConfigFile.h
#define MAX_PARAMETER_LIST_SIZE 10              // максимальное число параметров в списке
#define MAX_PARAMETER_NAME_STRING_SIZE 16       // максимальная длина имени параметра = 15 символов + null terminator
#define MAX_PARAMETER_VALUE_STRING_SIZE 20      // максимальная длина строкового значения параметра = 8 символов + null terminator

#define RW_MODE false
#define RO_MODE true


class Parameter {
    private:
        char name[MAX_PARAMETER_NAME_STRING_SIZE];
        char type;              // тип параметра: 'i'-целое; 's'-строковое обычная строка; ; 'm'-строка МАС адреса 
        char unit;              // едининца измерения параметра: 'n'-нет; 's'-секунды; 'm'-минуты
        // bool changed;
        uint8_t max_len;        // мах длина значения параметра в символах
        
        // bool b, defb;
        int32_t i, defi;
        int32_t ihi, ilo;
        // double f, deff;
        char s[MAX_PARAMETER_VALUE_STRING_SIZE];
        char defs[MAX_PARAMETER_VALUE_STRING_SIZE];

    public:
        Parameter() {}

        Parameter(const char *n, char aunit, uint8_t sz) : i(0), defi(0) {
            type = 'i';
            strcpy(name, n);
            strcpy(s, "");
            strcpy(defs, "");
            unit = aunit;
            max_len = sz;
            changed = false;
        }

        Parameter(const char *n, char aunit, uint8_t sz, int32_t v) : Parameter(n, aunit, sz) { type = 'i'; i = defi = v; }
        Parameter(const char *n, char aunit, uint8_t sz, const char *v) : Parameter(n, aunit, sz) {
            strcpy(s, v);
            strcpy(defs, v);
            // если третий символ значения строкового параметра равен ':', значит это МАС адрес
            type = s[2]==':'? 'm': 's';
        }
        Parameter(const char *n, char aunit, uint8_t sz, int32_t v, int32_t alo, int32_t ahi) : Parameter(n, aunit, sz) {
            type = 'i';
            i = defi = v; 
            ilo = alo;
            ihi = ahi;
        }
        
        void setValue(int32_t v) { i = v; }
        void setValue(const char *v) { strcpy(s, v); }
        
        void setDefault() {
            switch (type) {
                case 'i':
                    i = defi;
                    break;
                case 's':
                case 'm':
                    strcpy(s, defs);
                    break;
            }
        }

        void setHiLimitInt(int32_t v) { ihi = v; }

        void setLoLimitInt(int32_t v) { ilo = v; }

        const char *getName() { return (const char *) name; }
        
        char getType() { return type; }

        char getUnit() { return unit; }

        uint8_t getMaxLengtn() { return max_len; }

        int32_t getIntValue() { return i; }
        const char *getStringValue() { return (const char *) s; }

        bool isValidRange() {
            //----------------------------------------------------------------------------+
            //   Проверка значения параметра на допустимый диапазон и ограничение         |
            //  возвращает true, если значение параметра в допустимом диапазоне           |
            //  возвращает false, если значение параметра было изменено                   |
            //----------------------------------------------------------------------------+
            if (type == 'i') {
                if (i < ilo) {
                    i = ilo;
                    return false;
                }
                if (i > ihi) {
                    i = ihi;
                    return false;    
                }
            }
            return true;  
        }

        bool load_nvs(Preferences* nvs) {
            switch (type) {
                case 'i':
                    if (!nvs->isKey(name)) {

                        log_i("Not found name = %s", name);

                        if (!nvs->putLong(name, defi))
                            return false;
                    }
                    else
                    {
                        log_i("Found name = %s", name);
                    } i = nvs->getLong(name, defi);

                    log_i("i = %d", i);
                    break;
                case 's':
                case 'm':
                    if (!nvs->isKey(name)) {
                        log_i("Not found name = %s", name);
                        if (!nvs->putString(name, defs))
                            return false;
                    }
                    else
                    {
                        log_i("Found name = %s", name);
                    }
                    // nvs->getString(name, defs).toCharArray(s, MAX_PARAMETER_VALUE_STRING_SIZE);
                    nvs->getString(name, s, max_len+1);

                    log_i("s = %s", s);
                    break;
            }
            return true;
        }

        bool store_nvs(Preferences* nvs) {
            switch (type) {
                case 'i':
                    if (!nvs->putLong(name, i))
                        return false;
                    break;
                case 's':
                case 'm':
                    log_i("MAC = %s", s);
                    if (!nvs->putString(name, s))
                    {
                        log_i("Error write MAC !");
                        return false;
                    }
                    break;
            }
            return true;
        }

        bool changed;
};


class ListParameter {
    private:
        char name_space[MAX_PARAMETER_NAME_STRING_SIZE];            // название пространства имен для библиотеки Preferences в NVS
        Preferences nvs_params;                                     // экземпляр объекта Preferences для работы с NVS

    public:
        int8_t Count;                                               // количество параметров в списке
        Parameter *parameters[MAX_PARAMETER_LIST_SIZE];             // массив указателей на объекты параметров

        ListParameter(const char *ns) : Count(0) {
            strcpy(name_space, ns);
        }

        ~ListParameter() {
            for (uint8_t i = 0; i < Count; i++) {
                delete parameters[i];
            }
        }


        void addIntParameter(const char *name, char aunit, uint8_t mlen, int32_t value, int32_t alo, int32_t ahi) {
            parameters[Count] = new Parameter(name, aunit, mlen, value, alo, ahi);
            Count++;
        }

        void addIntParameter(const char *name, char aunit, uint8_t mlen, uint32_t value, uint32_t alo, uint32_t ahi) {
            addIntParameter(name, aunit, mlen, static_cast<int32_t>(value), static_cast<int32_t>(alo), static_cast<int32_t>(alo));
        }

        void addIntParameter(const char *name, char aunit, uint8_t mlen, int16_t value, int16_t alo, int16_t ahi) {
            addIntParameter(name, aunit, mlen, static_cast<int32_t>(value), static_cast<int32_t>(alo), static_cast<int32_t>(alo));
        }

        void addIntParameter(const char *name, char aunit, uint8_t mlen, uint16_t value, uint16_t alo, uint16_t ahi) {
            addIntParameter(name, aunit, mlen, static_cast<int32_t>(value), static_cast<int32_t>(alo), static_cast<int32_t>(alo));
        }

        void addIntParameter(const char *name, char aunit, uint8_t mlen, int8_t value, int8_t alo, int8_t ahi) {
            addIntParameter(name, aunit, mlen, static_cast<int32_t>(value), static_cast<int32_t>(alo), static_cast<int32_t>(alo));
        }

        void addIntParameter(const char *name, char aunit, uint8_t mlen, uint8_t value, uint8_t alo, uint8_t ahi) {
            addIntParameter(name, aunit, mlen, static_cast<int32_t>(value), static_cast<int32_t>(alo), static_cast<int32_t>(alo));
        }

        void addStringParameter(const char *name, char aunit, uint8_t mlen, const char *value) {
            parameters[Count] = new Parameter(name, aunit, mlen, value);
            Count++;
        }

        ////////// integer parameters

        void setIntParameter(const char *name, int32_t value) {
            Parameter *p = getParameter(name);
            if (p != NULL && p->getType() == 'i') p->setValue(value);
        }

        void setIntParameter(const char *name, uint32_t value) {
            setIntParameter(name, static_cast<int32_t>(value));
        }

        void setIntParameter(const char *name, int16_t value) {
            setIntParameter(name, static_cast<int32_t>(value));
        }

        void setIntParameter(const char *name, uint16_t value) {
            setIntParameter(name, static_cast<int32_t>(value));
        }

        void setIntParameter(const char *name, int8_t value) {
            setIntParameter(name, static_cast<int32_t>(value));
        }

        void setIntParameter(const char *name, uint8_t value) {
            setIntParameter(name, static_cast<int32_t>(value));
        }

        void setStringParameter(const char *name, const char *value) {
            Parameter *p = getParameter(name);
            if (p != NULL && (p->getType() == 's' || p->getType() == 'm')) p->setValue(value);
        }

        //************************************

        int32_t getIntParameter(const char *name) {
            Parameter *p = getParameter(name);
            if (p != NULL && p->getType() == 'i') return p->getIntValue();
            return 0;
        }

        const char *getStringParameter(const char *name) {
            Parameter *p = getParameter(name);
            if (p != NULL && (p->getType() == 's' || p->getType() == 'm')) return p->getStringValue();
            return "";
        }

        bool load();
        bool store();
        bool clear();

        // Parameter *getParameterByIndex(const uint8_t i) {
        //     if (Count && i < Count - 1)
        //         return parameters[i];
        //     log_e("Error: not found parameter with index %d", i);
        //     return NULL;
        // }

    private:
        Parameter *getParameter(const char *name) {
          for (uint8_t i = 0; i < Count; i++) {
                if (strcmp(parameters[i]->getName(), name) == 0) {
                    return parameters[i];
                }
            }
            return NULL;
        }
};



bool ListParameter::load() {

    // Open our namespace (or create it if it doesn't exist) in RW mode.
    if (!nvs_params.begin(name_space, RW_MODE)) {
        log_e("Error open NVS namespace");
        return false;    
    }

    if (!Count) {
        log_e("Error: List of parameters is empty");
        return false;    
    }

    log_i("Count = %d", Count);

    // char namepar[MAX_PARAMETER_NAME_STRING_SIZE];
    // strcpy(namepar, parameters[0]->getName());
    // log_i("Name parameter[0] = %s", namepar);    
    //     // переоткрываем пространство имен в режиме "read-write"
    //     if (!nvs_params.begin(name_space, RW_MODE)) {
    //         log_e("Error reopen NVS namespace");
    //         return false;    
    //     }


    for (uint8_t i = 0; i < Count; i++) {
        if (!parameters[i]->load_nvs(&nvs_params))
        {
            nvs_params.end();
            char namepar[MAX_PARAMETER_NAME_STRING_SIZE];
            strcpy(namepar, parameters[i]->getName());
            log_e("Error load parameter = %s", namepar);
            // log_i(namepar);
            return false;    
        }
    }

    // Параметры сохранены в NVS. Закрываем пространство имен в режиме "read-write".
    nvs_params.end();
    return true;
    

    // int num; // general purpose integer

    // // (1) print the string
    // Serial.println(name_space);

    // // (2) get the length of the string (excludes null terminator)
    // num = strlen(name_space);
    // Serial.print("String length is: ");
    // Serial.println(num);


    // // (3) get the length of the array (includes null terminator)
    // num = sizeof(name_space); // sizeof() is not a C string function
    // Serial.print("Size of the array: ");
    // Serial.println(num);


    // // (4) copy a string
    // strcpy(namepar, name_space);
    // Serial.println(namepar);


    
    // // Test for the existence of the "already initialized" key.
    // // bool tpInit = nvs_params.isKey("nvsInit");
    // return false;

}


bool ListParameter::store() {
    // Open our namespace (or create it if it doesn't exist) in RW mode.
    if (!nvs_params.begin(name_space, RW_MODE)) {
        log_e("Error open NVS namespace");
        return false;    
    }

    if (!Count) {
        log_e("Error: List of parameters is empty");
        return false;    
    }

    log_i("Count = %d", Count);

    bool result = true;

    for (uint8_t i = 0; i < Count; i++) {
        if (parameters[i]->changed)
            result &= parameters[i]->store_nvs(&nvs_params);
    }

    // Параметры сохранены в NVS. Закрываем пространство имен в режиме "read-write".
    nvs_params.end();
    return result;
}

#endif
