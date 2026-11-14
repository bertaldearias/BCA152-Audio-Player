#pragma once
#include <fstream>
#include <string>
#include <vector>
#include "json.hpp"
#include "Settings.hpp"

namespace app
{
    struct Animal_t
    {
        std::string animal;
        std::string audio;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Animal_t, animal, audio);

    class AppNav
    {
    private:
        int m_pos = 0;
        std::vector<Animal_t> m_animals;

    public:
        void init()
        {
            std::ifstream fs{"/spiffs/info.json"};
            m_animals = nlohmann::json::parse(fs);

            // restore the last animal; fall back to the first if the saved index is no longer valid
            m_pos = Settings::loadU8("idx", 0);
            if (m_pos >= (int)m_animals.size())
                m_pos = 0;
        }

        int index(void) const { return m_pos; }

        Animal_t &getCurrent(void) { return m_animals[m_pos]; }

        Animal_t &next(void)
        {
            m_pos = (m_pos + 1) % m_animals.size();
            return m_animals[m_pos];
        }

        Animal_t &prev(void)
        {
            m_pos = (m_pos + m_animals.size() - 1) % m_animals.size();
            return m_animals[m_pos];
        }
    };
}